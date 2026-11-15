#include <atomic>
#include <chrono>
#include <cstring>
#include <span>
#include <string>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "basement/transport/native/native_message_bus.h"
#include "basement/transport/rpc/rpc.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::transport::BinaryPayloadView;
using star::transport::MessageSerializer;
using star::transport::NativeMessageBus;
using star::transport::RpcClient;
using star::transport::RpcServer;
using star::transport::SerializedServiceRequest;
using star::transport::SerializedServiceResponse;
using star::transport::ServiceRequest;

struct EchoMsg {
    int value{0};

    auto encode_binary() const -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>(sizeof(int));
        std::memcpy(out.data(), &value, sizeof(int));
        return out;
    }

    static auto from_binary(std::span<const uint8_t> bytes) -> EchoMsg {
        auto msg = EchoMsg{};
        if (bytes.size() >= sizeof(int)) {
            std::memcpy(&msg.value, bytes.data(), sizeof(int));
        }
        return msg;
    }
};

auto make_serialized_request(int value) -> SerializedServiceRequest {
    auto request = star::transport::ServiceRequest{};
    auto msg = std::make_shared<const EchoMsg>(EchoMsg{value});
    request.body.payload = TypedConstPtr::from(msg);
    request.body.correlation_id = star::transport::CorrelationId::generate();

    auto serializer = MessageSerializer{};
    serializer.payload_size_hint = sizeof(int);
    serializer.encode_binary = [msg]() { return msg->encode_binary(); };
    return SerializedServiceRequest{request, serializer};
}


// 每个测试起一条普通 MessageBus + 一个 RpcServer；客户端直接用普通通道
struct EchoServer {
    std::shared_ptr<NativeMessageBus> bus;
    std::unique_ptr<RpcServer> server;

    explicit EchoServer(const std::string& name, std::atomic<int>* handler_calls) {
        bus = std::make_shared<NativeMessageBus>();
        server = std::make_unique<RpcServer>(*bus, name, [handler_calls](const ServiceRequest& request) -> SerializedServiceResponse {
            if (handler_calls != nullptr) {
                handler_calls->fetch_add(1);
            }
            auto bytes = std::vector<uint8_t>{};
            if (const auto binary = request.body.payload.as<BinaryPayloadView>()) {
                bytes.assign(binary->data, binary->data + binary->size);
            }
            auto response = star::transport::ServiceResponse{};
            response.ok = true;
            auto owner = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
            response.body.payload = TypedConstPtr::from(owner);
            auto serializer = MessageSerializer{};
            serializer.payload_size_hint = owner->size();
            serializer.encode_binary = [owner]() { return *owner; };
            return SerializedServiceResponse{response, std::move(serializer)};
        });
    }
};

// design_remedy G1：并发 N 路请求，确保无死锁/丢响应/超时
TEST_CASE("rpc over message bus handles concurrent synchronous callers") {
    auto handler_calls = std::atomic<int>{0};
    auto server = EchoServer{"/test/rpc/sync", &handler_calls};

    constexpr int kThreads = 8;
    constexpr int kCallsPerThread = 100;

    auto ok_count = std::atomic<int>{0};
    auto fail_count = std::atomic<int>{0};
    auto bad_body = std::atomic<int>{0};
    auto threads = std::vector<std::thread>{};
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            auto client = RpcClient{*server.bus, "/test/rpc/sync"};
            for (int i = 0; i < kCallsPerThread; ++i) {
                const auto expected = t * kCallsPerThread + i;
                const auto response = client.call_serialized(make_serialized_request(expected));
                if (not response.ok) {
                    fail_count.fetch_add(1);
                    continue;
                }
                const auto binary = response.body.payload.as<BinaryPayloadView>();
                if (not binary or EchoMsg::from_binary(binary->bytes()).value != expected) {
                    bad_body.fetch_add(1);
                    continue;
                }
                ok_count.fetch_add(1);
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(fail_count.load(), 0);
    REQUIRE_EQ(bad_body.load(), 0);
    REQUIRE_EQ(ok_count.load(), kThreads * kCallsPerThread);
    REQUIRE_EQ(handler_calls.load(), kThreads * kCallsPerThread);
}

TEST_CASE("rpc over message bus handles concurrent async callers") {
    auto handler_calls = std::atomic<int>{0};
    auto server = EchoServer{"/test/rpc/async", &handler_calls};

    constexpr int kThreads = 6;
    constexpr int kInFlight = 32;

    auto ok_count = std::atomic<int>{0};
    auto fail_count = std::atomic<int>{0};
    auto threads = std::vector<std::thread>{};
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            auto client = RpcClient{*server.bus, "/test/rpc/async"};
            for (int batch = 0; batch < 5; ++batch) {
                auto futures = std::vector<std::future<star::transport::ServiceResponse>>{};
                futures.reserve(kInFlight);
                for (int i = 0; i < kInFlight; ++i) {
                    futures.push_back(client.async_call_serialized(make_serialized_request(t * 1000 + batch * kInFlight + i)));
                }
                for (auto& future : futures) {
                    if (future.get().ok) {
                        ok_count.fetch_add(1);
                    } else {
                        fail_count.fetch_add(1);
                    }
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    REQUIRE_EQ(fail_count.load(), 0);
    REQUIRE_EQ(ok_count.load(), kThreads * 5 * kInFlight);
    REQUIRE_EQ(handler_calls.load(), kThreads * 5 * kInFlight);
}

TEST_CASE("rpc client reports channel availability") {
    auto server = EchoServer{"/test/rpc/availability", nullptr};

    auto known = RpcClient{*server.bus, "/test/rpc/availability"};
    auto unknown = RpcClient{*server.bus, "/test/rpc/missing"};

    REQUIRE(known.available());
    REQUIRE(not unknown.available());
}

TEST_CASE("rpc server rejects invalid setup") {
    auto bus = NativeMessageBus{};

    auto threw = false;
    try {
        (void)RpcServer{bus, "", [](const ServiceRequest&) { return SerializedServiceResponse{}; }};
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);

    threw = false;
    try {
        (void)RpcServer{bus, "/test/rpc/nohandler", nullptr};
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

// 决策四的核心收益：服务流量就是普通通道流量——统计里必须看得到，
// 这意味着录制、monitor、QoS、类型校验都自动覆盖服务，不需要第二套实现
TEST_CASE("rpc traffic appears in ordinary channel statistics") {
    auto server = EchoServer{"/test/rpc/stats", nullptr};
    auto client = RpcClient{*server.bus, "/test/rpc/stats"};

    const auto response = client.call_serialized(make_serialized_request(1));
    REQUIRE(response.ok);

    const auto request_stats = server.bus->channel_stats("/test/rpc/stats/request");
    REQUIRE(request_stats.has_value());
    REQUIRE(request_stats->write_count >= 1);
    REQUIRE(request_stats->deliver_count >= 1);

    const auto response_stats = server.bus->channel_stats("/test/rpc/stats/response");
    REQUIRE(response_stats.has_value());
    REQUIRE(response_stats->write_count >= 1);
}

// 服务端 handler 抛异常必须回传为 ok=false，而不是让客户端 5 秒超时
TEST_CASE("rpc server reports handler exceptions to the caller") {
    auto bus = std::make_shared<NativeMessageBus>();
    auto server = RpcServer{*bus, "/test/rpc/throws", [](const ServiceRequest&) -> SerializedServiceResponse {
                            throw star::excep::RuntimeError("handler exploded");
                        }};
    auto client = RpcClient{*bus, "/test/rpc/throws"};

    const auto response = client.call_serialized(make_serialized_request(1));
    REQUIRE(not response.ok);
    REQUIRE(response.message.find("handler exploded") != std::string::npos);
}

}  // namespace

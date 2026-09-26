#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include "basement/transport/native/native_service_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::transport::NativeServiceBus;
using star::transport::SerializedServiceResponse;
using star::transport::ServiceDescriptor;
using star::transport::ServiceRequest;

struct EchoMsg {
    int value{0};
};

auto make_bus_with_echo(const std::string& name, std::atomic<int>* handler_calls) -> std::pair<std::shared_ptr<NativeServiceBus>, std::shared_ptr<star::transport::ServiceServer>> {
    const auto bus = std::make_shared<NativeServiceBus>();
    auto descriptor = ServiceDescriptor{};
    descriptor.name = name;
    const auto server = bus->create_server(descriptor, [handler_calls](const ServiceRequest& request) {
        if (handler_calls != nullptr) {
            handler_calls->fetch_add(1);
        }
        auto response = SerializedServiceResponse{};
        response.response.ok = true;
        response.response.body = request.body;
        return response;
    });
    return {bus, server};
}

auto make_request(int value) -> ServiceRequest {
    auto request = ServiceRequest{};
    request.body.payload = TypedConstPtr::from(std::make_shared<const EchoMsg>(EchoMsg{value}));
    return request;
}

// design_remedy G1：并发 N 路请求，确保无死锁/丢响应/超时回潮
TEST_CASE("native service bus handles concurrent synchronous callers") {
    auto handler_calls = std::atomic<int>{0};
    const auto [bus, server] = make_bus_with_echo("test.service.sync", &handler_calls);

    constexpr int kThreads = 8;
    constexpr int kCallsPerThread = 100;

    auto ok_count = std::atomic<int>{0};
    auto fail_count = std::atomic<int>{0};
    auto bad_body = std::atomic<int>{0};
    auto threads = std::vector<std::thread>{};
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            const auto client = bus->create_client(ServiceDescriptor{"test.service.sync"});
            for (int i = 0; i < kCallsPerThread; ++i) {
                const auto response = client->call(make_request(t * kCallsPerThread + i));
                if (not response.ok) {
                    fail_count.fetch_add(1);
                    continue;
                }
                const auto echoed = response.body.payload.as<EchoMsg>();
                if (not echoed || echoed->value != t * kCallsPerThread + i) {
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

TEST_CASE("native service bus handles concurrent async callers") {
    auto handler_calls = std::atomic<int>{0};
    const auto [bus, server] = make_bus_with_echo("test.service.async", &handler_calls);

    constexpr int kThreads = 6;
    constexpr int kInFlight = 32;

    auto ok_count = std::atomic<int>{0};
    auto fail_count = std::atomic<int>{0};
    auto threads = std::vector<std::thread>{};
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            const auto client = bus->create_client(ServiceDescriptor{"test.service.async"});
            for (int batch = 0; batch < 5; ++batch) {
                auto futures = std::vector<std::future<star::transport::ServiceResponse>>{};
                futures.reserve(kInFlight);
                for (int i = 0; i < kInFlight; ++i) {
                    futures.push_back(client->async_call(make_request(t * 1000 + batch * kInFlight + i)));
                }
                for (auto& future : futures) {
                    const auto response = future.get();
                    if (response.ok) {
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

TEST_CASE("native service client reports availability") {
    const auto [bus, server] = make_bus_with_echo("test.service.availability", nullptr);

    const auto known = bus->create_client(ServiceDescriptor{"test.service.availability"});
    const auto unknown = bus->create_client(ServiceDescriptor{"test.service.missing"});

    REQUIRE(known->available());
    REQUIRE(not unknown->available());
}

TEST_CASE("native service bus rejects invalid server setup") {
    const auto bus = std::make_shared<NativeServiceBus>();

    auto threw = false;
    try {
        (void)bus->create_server(ServiceDescriptor{}, [](const ServiceRequest&) { return SerializedServiceResponse{}; });
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);

    threw = false;
    try {
        (void)bus->create_server(ServiceDescriptor{"test.service.nohandler"}, nullptr);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

}  // namespace

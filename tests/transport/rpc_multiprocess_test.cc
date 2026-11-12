#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <poll.h>
#include <string>
#include <thread>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "basement/transport/rpc/rpc.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::transport::BinaryPayloadView;
using star::transport::MessageSerializer;
using star::transport::RpcClient;
using star::transport::RpcServer;
using star::transport::SerializedMessage;
using star::transport::SerializedServiceRequest;
using star::transport::SerializedServiceResponse;
using star::transport::ServiceRequest;
using star::transport::ShmRoutedMessageBus;

auto make_serialized(std::vector<uint8_t> bytes) -> SerializedMessage {
    auto owner = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
    auto message = SerializedMessage{};
    message.view.payload = TypedConstPtr::from(owner);
    message.serializer.payload_size_hint = owner->size();
    message.serializer.encode_binary = [owner]() { return *owner; };
    return message;
}

auto make_request_message(std::vector<uint8_t> bytes) -> SerializedServiceRequest {
    auto serialized = make_serialized(std::move(bytes));
    auto request = star::transport::ServiceRequest{};
    request.body = serialized.view;
    return SerializedServiceRequest{request, serialized.serializer};
}

auto read_ready_byte(int fd, int timeout_ms) -> bool {
    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    const auto ready = ::poll(&pfd, 1, timeout_ms);
    if (ready <= 0) {
        return false;
    }
    char value = 0;
    return ::read(fd, &value, 1) == 1;
}

// 子进程：起 shm server，等父进程发完成信号后正常析构退出（让段被 unlink）
auto run_server_child(const std::string& service,
                      const std::string& route_plan,
                      int ready_fd,
                      int done_fd) -> int {
    auto bus = std::make_shared<ShmRoutedMessageBus>();
    if (not bus->shm_available()) {
        return 3;
    }
    bus->configure_route_registry("server", route_plan);
    auto server = RpcServer{*bus, service, [](const ServiceRequest& request) -> SerializedServiceResponse {
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
    }};

    const char ready = 'R';
    if (::write(ready_fd, &ready, 1) != 1) {
        return 4;
    }

    struct pollfd pfd {};
    pfd.fd = done_fd;
    pfd.events = POLLIN;
    if (::poll(&pfd, 1, 10000) <= 0) {
        return 5;
    }
    char done = 0;
    return ::read(done_fd, &done, 1) == 1 ? 0 : 6;
}

}  // namespace

// P1-1 遗留的最后一个验证缺口：services 并入通道后，客户端与服务端在**真正不同的
// 进程**里，经 shm 路由完成 request/response。此前只有同进程或「两个总线模拟两进程」。
TEST_CASE("rpc works across two processes over shm") {
    const auto unique = std::to_string(static_cast<long>(::getpid())) + "_" +
                        std::to_string(static_cast<long>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xffff));
    const auto service = "test.rpc.multiproc." + unique;
    const auto request_channel = service + "/request";
    const auto response_channel = service + "/response";
    // 客户端生产请求、服务端消费；服务端生产响应、客户端消费
    const auto route_plan = request_channel + "|client|server;" + response_channel + "|server|client";

    int ready_pipe[2]{};
    int done_pipe[2]{};
    REQUIRE(::pipe(ready_pipe) == 0);
    REQUIRE(::pipe(done_pipe) == 0);

    const auto pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::close(ready_pipe[0]);
        ::close(done_pipe[1]);
        const auto rc = run_server_child(service, route_plan, ready_pipe[1], done_pipe[0]);
        ::close(ready_pipe[1]);
        ::close(done_pipe[0]);
        ::_exit(rc);
    }

    ::close(ready_pipe[1]);
    ::close(done_pipe[0]);
    REQUIRE(read_ready_byte(ready_pipe[0], 5000));

    auto client_bus = std::make_shared<ShmRoutedMessageBus>();
    REQUIRE(client_bus->shm_available());
    client_bus->configure_route_registry("client", route_plan);
    auto client = RpcClient{*client_bus, service, 3000};

    const auto payload = std::vector<uint8_t>{0x11, 0x22, 0x33, 0x44};
    const auto response = client.call_serialized(make_request_message(payload));
    REQUIRE(response.ok);

    const auto echoed = response.body.payload.as<BinaryPayloadView>();
    REQUIRE(echoed != nullptr);
    REQUIRE_EQ(std::vector<uint8_t>(echoed->data, echoed->data + echoed->size), payload);

    const char done = 'D';
    REQUIRE(::write(done_pipe[1], &done, 1) == 1);
    ::close(done_pipe[1]);
    ::close(ready_pipe[0]);

    int status = 0;
    REQUIRE(::waitpid(pid, &status, 0) == pid);
    REQUIRE(WIFEXITED(status));
    REQUIRE_EQ(WEXITSTATUS(status), 0);
}

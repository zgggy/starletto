#ifndef __TRANSPORT_RPC_H__
#define __TRANSPORT_RPC_H__

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/shm/shm_views.h"

namespace star::transport {

// RPC = 通道之上的「请求-响应关联」糖，不是第四种传输。
//
// <service>/request 与 <service>/response 是普通通道：自动进路由计划、自动可录制、
// 类型校验与解码策略全部复用通道层。这里只保留每个组件自己写都容易写错的一小块：
// correlation id 关联、pending 表、超时清理、错误回传。
//
// 线上格式（payload 内部信封，不是 schema）：
//   u64 correlation_id | u8 ok | u32 message_len | message | body
// 请求的 ok 恒为 1、message 为空；响应携带错误信息。body 是用户消息的二进制编码。

static constexpr int64_t kDefaultRpcTimeoutMs{5000};
static constexpr size_t kRpcEnvelopeHeaderSize = sizeof(uint64_t) + sizeof(uint8_t) + sizeof(uint32_t);

struct ServiceDescriptor {
    std::string name;
};

struct ServiceRequest {
    MessageView body;
};

struct ServiceResponse {
    bool ok{true};
    std::string message;
    MessageView body;
};

struct SerializedServiceRequest {
    ServiceRequest request;
    MessageSerializer serializer;
};

struct SerializedServiceResponse {
    ServiceResponse response;
    MessageSerializer serializer;
};

struct RpcEnvelope {
    CorrelationId correlation_id;
    bool ok{true};
    std::string message;
    std::shared_ptr<std::vector<uint8_t>> owner;
    size_t body_offset{0};
    size_t body_size{0};

    auto body_bytes() const -> std::span<const uint8_t> {
        if (not owner or body_size == 0) {
            return {};
        }
        return std::span<const uint8_t>(owner->data() + body_offset, body_size);
    }
};

inline auto encode_rpc_envelope(const CorrelationId& correlation_id,
                                bool ok,
                                const std::string& message,
                                std::span<const uint8_t> body) -> std::vector<uint8_t> {
    auto out = std::vector<uint8_t>(kRpcEnvelopeHeaderSize + message.size() + body.size(), 0);
    auto* p = out.data();
    std::memcpy(p, &correlation_id.value, sizeof(correlation_id.value));
    p += sizeof(correlation_id.value);
    const auto flag = static_cast<uint8_t>(ok ? 1 : 0);
    std::memcpy(p, &flag, sizeof(flag));
    p += sizeof(flag);
    const auto message_len = static_cast<uint32_t>(message.size());
    std::memcpy(p, &message_len, sizeof(message_len));
    p += sizeof(message_len);
    if (not message.empty()) {
        std::memcpy(p, message.data(), message.size());
        p += message.size();
    }
    if (not body.empty()) {
        std::memcpy(p, body.data(), body.size());
    }
    return out;
}

inline auto decode_rpc_envelope(std::shared_ptr<std::vector<uint8_t>> bytes) -> RpcEnvelope {
    auto out = RpcEnvelope{};
    out.owner = std::move(bytes);
    if (not out.owner or out.owner->size() < kRpcEnvelopeHeaderSize) {
        throw star::excep::RuntimeError("rpc envelope too small");
    }
    const auto* p = out.owner->data();
    std::memcpy(&out.correlation_id.value, p, sizeof(out.correlation_id.value));
    p += sizeof(out.correlation_id.value);
    uint8_t flag{1};
    std::memcpy(&flag, p, sizeof(flag));
    p += sizeof(flag);
    uint32_t message_len{0};
    std::memcpy(&message_len, p, sizeof(message_len));
    p += sizeof(message_len);
    if (out.owner->size() < kRpcEnvelopeHeaderSize + message_len) {
        throw star::excep::RuntimeError("rpc envelope message length overflows payload");
    }
    out.ok = flag != 0;
    out.message.assign(reinterpret_cast<const char*>(p), message_len);
    out.body_offset = kRpcEnvelopeHeaderSize + message_len;
    out.body_size = out.owner->size() - out.body_offset;
    return out;
}

// 从 MessageView 取一份字节拷贝：shm 槽位在本轮回调返回后可能被覆写，
// 请求/响应体都可能被持有超过该窗口，必须拷出来再解码。
inline auto copy_message_bytes(const MessageView& message) -> std::shared_ptr<std::vector<uint8_t>> {
    if (const auto binary = message.payload.as<BinaryPayloadView>()) {
        return std::make_shared<std::vector<uint8_t>>(binary->data, binary->data + binary->size);
    }
    if (const auto slot = message.payload.as<ShmSlotView>()) {
        if (not slot->stable()) {
            throw star::excep::RuntimeError("rpc payload dropped, shm slot was overwritten before decode");
        }
        const auto bytes = slot->bytes();
        return std::make_shared<std::vector<uint8_t>>(bytes.begin(), bytes.end());
    }
    throw star::excep::RuntimeError("rpc payload is not a serialized message");
}

// 把一条消息编码成字节：优先二进制 TLV（可演进），其次已有的字节视图，最后固定布局。
inline auto serialize_message_bytes(const MessageView& body, const MessageSerializer& serializer) -> std::vector<uint8_t> {
    if (serializer.encode_binary) {
        return serializer.encode_binary();
    }
    if (const auto binary = body.payload.as<BinaryPayloadView>()) {
        return std::vector<uint8_t>(binary->data, binary->data + binary->size);
    }
    if (const auto slot = body.payload.as<ShmSlotView>()) {
        const auto bytes = slot->bytes();
        return std::vector<uint8_t>(bytes.begin(), bytes.end());
    }
    if (serializer.encode_into) {
        auto buffer = std::vector<uint8_t>(serializer.payload_size_hint > 0 ? serializer.payload_size_hint : (1u << 20));
        const auto size = serializer.encode_into(buffer.data(), buffer.size());
        buffer.resize(size);
        return buffer;
    }
    throw star::excep::RuntimeError("rpc message has no serializable payload");
}

inline auto make_rpc_serialized_message(std::shared_ptr<std::vector<uint8_t>> bytes, const star::schema::SchemaTypeId& type_id) -> SerializedMessage {
    auto serialized = SerializedMessage{};
    serialized.view.payload = star::base::TypedConstPtr::from(std::make_shared<BinaryPayloadView>(
        BinaryPayloadView{bytes, bytes->data(), bytes->size()}));
    serialized.view.schema_type_id = type_id;
    serialized.serializer.payload_size_hint = bytes->size();
    serialized.serializer.encode_binary = [bytes]() { return *bytes; };
    return serialized;
}

// 客户端：基于普通 Writer/Reader 实现请求-响应关联
class RpcClient final {
  public:
    RpcClient(MessageBus& bus, std::string service_name, int64_t timeout_ms = kDefaultRpcTimeoutMs)
        : bus_(&bus),
          service_name_(std::move(service_name)),
          request_channel_(service_name_ + "/request"),
          response_channel_(service_name_ + "/response"),
          timeout_ms_(timeout_ms) {
        if (service_name_.empty()) {
            throw star::excep::RuntimeError("RpcClient failed, empty service name");
        }
        state_ = std::make_shared<SharedState>();
        state_->service_name = service_name_;
    }

    ~RpcClient() { shutdown(); }

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    auto available() const -> bool {
        const auto channels = bus_->declared_channels();
        return std::find(channels.begin(), channels.end(), request_channel_) != channels.end();
    }

    // 不带 serializer 的调用只适用于请求体已经是字节视图（BinaryPayloadView/ShmSlotView）
    auto call(const ServiceRequest& request) -> ServiceResponse {
        return call_serialized(SerializedServiceRequest{request, MessageSerializer{}});
    }

    auto call_serialized(const SerializedServiceRequest& request) -> ServiceResponse {
        auto started = start_call_serialized(request);
        if (started.future.wait_for(std::chrono::milliseconds(timeout_ms_)) != std::future_status::ready) {
            cancel_call(started.correlation_id, "timeout waiting for response");
            return ServiceResponse{false, "timeout waiting for response", {}};
        }
        return started.future.get();
    }

    auto async_call_serialized(const SerializedServiceRequest& request) -> std::future<ServiceResponse> {
        return start_call_serialized(request).future;
    }

  private:
    struct StartedCall {
        CorrelationId correlation_id;
        std::future<ServiceResponse> future;
    };

    // 回调生命周期必须独立于 RpcClient 对象：native 总线会在写入线程上同步分发，
    // 多个客户端共享同一响应通道时，A 的响应也会被投递给 B..N。若回调捕获 this，
    // 任一客户端析构后其它线程的在途分发就会踩到已销毁的 mutex（SIGSEGV）。
    struct SharedState {
        std::mutex mutex;
        std::unordered_map<CorrelationId, std::shared_ptr<std::promise<ServiceResponse>>> pending;
        bool alive{true};
        std::string service_name;
    };

    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->alive = false;
            for (auto& [_, promise] : state_->pending) {
                if (promise) {
                    promise->set_value(ServiceResponse{false, "client destroyed", {}});
                }
            }
            state_->pending.clear();
        }
        std::lock_guard<std::mutex> lock(reader_mutex_);
        response_reader_.reset();
    }

    auto start_call_serialized(const SerializedServiceRequest& request) -> StartedCall {
        const auto correlation_id = request.request.body.correlation_id.is_valid()
                                        ? request.request.body.correlation_id
                                        : CorrelationId::generate();

        auto body_bytes = serialize_message_bytes(request.request.body, request.serializer);
        auto envelope = std::make_shared<std::vector<uint8_t>>(
            encode_rpc_envelope(correlation_id, true, "", std::span<const uint8_t>(body_bytes.data(), body_bytes.size())));

        auto promise = std::make_shared<std::promise<ServiceResponse>>();
        auto future = promise->get_future();

        ensure_response_reader();

        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (not state_->alive) {
                promise->set_value(ServiceResponse{false, "client is shutting down", {}});
                return StartedCall{correlation_id, std::move(future)};
            }
            state_->pending[correlation_id] = promise;
        }

        auto message = make_rpc_serialized_message(envelope, request.request.body.schema_type_id);
        message.view.correlation_id = correlation_id;
        auto descriptor = ChannelDescriptor{};
        descriptor.name = request_channel_;
        descriptor.schema_type_id = request.request.body.schema_type_id;
        auto writer = bus_->create_writer(descriptor);
        if (not writer) {
            cancel_call(correlation_id, "failed to create request writer");
            return StartedCall{correlation_id, std::move(future)};
        }
        const auto result = writer->write_serialized(message);
        if (not result.ok()) {
            cancel_call(correlation_id, std::string("request write failed: ") + write_status_string(result.status));
        }
        return StartedCall{correlation_id, std::move(future)};
    }

    void ensure_response_reader() {
        std::lock_guard<std::mutex> lock(reader_mutex_);
        if (response_reader_) {
            return;
        }
        auto descriptor = ChannelDescriptor{};
        descriptor.name = response_channel_;
        auto state = state_;
        response_reader_ = bus_->create_reader(descriptor, [state](const MessageView& response) { on_response(state, response); });
    }

    static void on_response(const std::shared_ptr<SharedState>& state, const MessageView& response) {
        std::shared_ptr<std::promise<ServiceResponse>> promise;
        RpcEnvelope envelope;
        try {
            envelope = decode_rpc_envelope(copy_message_bytes(response));
        } catch (const std::exception& e) {
            logE << "RpcClient dropped malformed response: service=" << state->service_name << ", error=" << e.what();
            return;
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (not state->alive) {
                return;
            }
            const auto it = state->pending.find(envelope.correlation_id);
            if (it == state->pending.end()) {
                return;
            }
            promise = std::exchange(it->second, nullptr);
            state->pending.erase(it);
        }
        if (not promise) {
            return;
        }
        if (not envelope.ok) {
            promise->set_value(ServiceResponse{false, envelope.message, {}});
            return;
        }
        auto body = MessageView{};
        auto body_owner = std::make_shared<BinaryPayloadView>();
        body_owner->owner = envelope.owner;
        body_owner->data = envelope.owner->data() + envelope.body_offset;
        body_owner->size = envelope.body_size;
        body.payload = star::base::TypedConstPtr::from(std::move(body_owner));
        body.schema_type_id = response.schema_type_id;
        body.correlation_id = envelope.correlation_id;
        promise->set_value(ServiceResponse{true, "", body});
    }

    void cancel_call(const CorrelationId& correlation_id, std::string_view reason) {
        std::shared_ptr<std::promise<ServiceResponse>> promise;
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            const auto it = state_->pending.find(correlation_id);
            if (it == state_->pending.end()) {
                return;
            }
            promise = std::exchange(it->second, nullptr);
            state_->pending.erase(it);
        }
        if (promise) {
            promise->set_value(ServiceResponse{false, std::string(reason), {}});
        }
    }

    MessageBus* bus_;
    std::string service_name_;
    std::string request_channel_;
    std::string response_channel_;
    int64_t timeout_ms_;
    std::shared_ptr<SharedState> state_;

    std::mutex reader_mutex_;
    std::shared_ptr<Reader> response_reader_;
};

// 服务端：订阅 <service>/request，handler 结果写回 <service>/response
class RpcServer final {
  public:
    using Handler = std::function<SerializedServiceResponse(const ServiceRequest&)>;

    RpcServer(MessageBus& bus, std::string service_name, Handler handler) {
        if (service_name.empty()) {
            throw star::excep::RuntimeError("RpcServer failed, empty service name");
        }
        if (not handler) {
            throw star::excep::RuntimeError("RpcServer failed, empty handler: ", service_name);
        }
        state_ = std::make_shared<ServerState>();
        state_->bus = &bus;
        state_->service_name = std::move(service_name);
        state_->handler = std::move(handler);
        start();
    }

    ~RpcServer() { stop(); }

    RpcServer(const RpcServer&) = delete;
    RpcServer& operator=(const RpcServer&) = delete;

    void start() {
        auto descriptor = ChannelDescriptor{};
        descriptor.name = state_->service_name + "/request";
        auto state = state_;
        reader_ = state_->bus->create_reader(descriptor, [state](const MessageView& request) { handle_request(state, request); });
    }

    void stop() {
        if (not state_) {
            return;
        }
        state_->alive.store(false, std::memory_order_relaxed);
        reader_.reset();
    }

  private:
    struct ServerState {
        MessageBus* bus{nullptr};
        std::string service_name;
        Handler handler;
        std::atomic<bool> alive{true};
    };

    // 回调捕获 shared state 而不是 this：reader 析构无法中断「已被总线拷贝、正在另一个
    // 线程执行」的回调，捕获 this 会在服务端析构后踩到已销毁成员。
    static void handle_request(const std::shared_ptr<ServerState>& state, const MessageView& message) {
        if (not state->alive.load(std::memory_order_relaxed)) {
            return;
        }
        RpcEnvelope envelope;
        try {
            envelope = decode_rpc_envelope(copy_message_bytes(message));
        } catch (const std::exception& e) {
            logE << "RpcServer dropped malformed request: service=" << state->service_name << ", error=" << e.what();
            return;
        }

        auto request = ServiceRequest{};
        auto body_owner = std::make_shared<BinaryPayloadView>();
        body_owner->owner = envelope.owner;
        body_owner->data = envelope.owner->data() + envelope.body_offset;
        body_owner->size = envelope.body_size;
        request.body.payload = star::base::TypedConstPtr::from(std::move(body_owner));
        request.body.schema_type_id = message.schema_type_id;
        request.body.correlation_id = envelope.correlation_id;
        request.body.decode_policy = message.decode_policy;

        auto response = SerializedServiceResponse{};
        try {
            response = state->handler(request);
        } catch (const std::exception& e) {
            response.response.ok = false;
            response.response.message = e.what();
        } catch (...) {
            response.response.ok = false;
            response.response.message = "handler exception";
        }

        std::vector<uint8_t> body_bytes;
        if (response.response.ok and response.response.body.payload.ptr != nullptr) {
            try {
                body_bytes = serialize_message_bytes(response.response.body, response.serializer);
            } catch (const std::exception& e) {
                response.response.ok = false;
                response.response.message = e.what();
            }
        }
        auto envelope_bytes = std::make_shared<std::vector<uint8_t>>(
            encode_rpc_envelope(envelope.correlation_id,
                                response.response.ok,
                                response.response.message,
                                std::span<const uint8_t>(body_bytes.data(), body_bytes.size())));

        auto descriptor = ChannelDescriptor{};
        descriptor.name = state->service_name + "/response";
        descriptor.schema_type_id = response.response.ok ? response.response.body.schema_type_id : star::schema::SchemaTypeId{};
        auto writer = state->bus->create_writer(descriptor);
        if (not writer) {
            logE << "RpcServer response writer unavailable: service=" << state->service_name;
            return;
        }
        auto reply = make_rpc_serialized_message(envelope_bytes, descriptor.schema_type_id);
        reply.view.correlation_id = envelope.correlation_id;
        const auto result = writer->write_serialized(reply);
        if (not result.ok()) {
            logE << "RpcServer response write failed: service=" << state->service_name
                 << ", status=" << write_status_string(result.status) << ", err=" << result.error_message;
        }
    }

    std::shared_ptr<ServerState> state_;
    std::shared_ptr<Reader> reader_;
};

}  // namespace star::transport

#endif  // __TRANSPORT_RPC_H__

#ifndef __SHM_SERVICE_BUS_H__
#define __SHM_SERVICE_BUS_H__

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/transport/api/message_bus.h"
#include "basement/transport/api/service_bus.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace star::transport {

static constexpr int64_t kDefaultServiceTimeoutMs{5000};
static constexpr size_t kServiceEnvelopeHeaderSize{sizeof(uint64_t)};

struct ServiceEnvelope {
    CorrelationId correlation_id;
    MessageView body;
};

inline auto encode_service_envelope_binary(CorrelationId correlation_id, const MessageSerializer& serializer) -> std::vector<uint8_t> {
    auto payload = serializer.encode_binary ? serializer.encode_binary() : std::vector<uint8_t>{};
    auto out = std::vector<uint8_t>(kServiceEnvelopeHeaderSize + payload.size(), 0);
    std::memcpy(out.data(), &correlation_id.value, sizeof(correlation_id.value));
    if (!payload.empty()) {
        std::memcpy(out.data() + kServiceEnvelopeHeaderSize, payload.data(), payload.size());
    }
    return out;
}

inline auto encode_service_envelope_into(CorrelationId correlation_id, const MessageSerializer& serializer, uint8_t* buf, size_t cap) -> size_t {
    if (buf == nullptr || cap < kServiceEnvelopeHeaderSize) {
        return 0;
    }
    std::memcpy(buf, &correlation_id.value, sizeof(correlation_id.value));
    if (serializer.encode_into) {
        const auto payload_size = serializer.encode_into(buf + kServiceEnvelopeHeaderSize, cap - kServiceEnvelopeHeaderSize);
        return payload_size == 0 ? 0 : kServiceEnvelopeHeaderSize + payload_size;
    }
    if (serializer.encode_binary) {
        const auto payload = serializer.encode_binary();
        if (kServiceEnvelopeHeaderSize + payload.size() > cap) {
            return 0;
        }
        if (!payload.empty()) {
            std::memcpy(buf + kServiceEnvelopeHeaderSize, payload.data(), payload.size());
        }
        return kServiceEnvelopeHeaderSize + payload.size();
    }
    return kServiceEnvelopeHeaderSize;
}

inline auto decode_service_envelope(const MessageView& message) -> ServiceEnvelope {
    auto envelope = ServiceEnvelope{};
    envelope.body = message;
    if (const auto slot = message.payload.as<ShmSlotView>()) {
        if (slot->size < kServiceEnvelopeHeaderSize) {
            throw star::excep::RuntimeError("ShmServiceBus decode failed, envelope too small");
        }
        if (not slot->stable()) {
            throw star::excep::RuntimeError("ShmServiceBus decode failed, slot overwritten before decode");
        }
        std::memcpy(&envelope.correlation_id.value, slot->data, sizeof(envelope.correlation_id.value));
        auto payload_view = std::make_shared<ShmSlotView>();
        payload_view->mapping = slot->mapping;
        payload_view->data = slot->data + kServiceEnvelopeHeaderSize;
        payload_view->size = slot->size - kServiceEnvelopeHeaderSize;
        payload_view->channel_name = slot->channel_name;
        payload_view->schema_type_id = slot->schema_type_id;
        // 派生视图指向同一槽位，必须继承代际信息，否则丢掉复核能力
        payload_view->seq_addr = slot->seq_addr;
        payload_view->observed_seq = slot->observed_seq;
        payload_view->slot_count = slot->slot_count;
        envelope.body.payload = star::base::TypedConstPtr::from(std::move(payload_view));
        envelope.body.correlation_id = envelope.correlation_id;
        return envelope;
    }
    if (const auto binary = message.payload.as<BinaryPayloadView>()) {
        if (binary->size < kServiceEnvelopeHeaderSize) {
            throw star::excep::RuntimeError("ShmServiceBus decode failed, binary envelope too small");
        }
        std::memcpy(&envelope.correlation_id.value, binary->data, sizeof(envelope.correlation_id.value));
        auto payload_view = std::make_shared<BinaryPayloadView>();
        payload_view->owner = binary->owner;
        payload_view->data = binary->data + kServiceEnvelopeHeaderSize;
        payload_view->size = binary->size - kServiceEnvelopeHeaderSize;
        envelope.body.payload = star::base::TypedConstPtr::from(std::move(payload_view));
        envelope.body.correlation_id = envelope.correlation_id;
        return envelope;
    }
    envelope.correlation_id = message.correlation_id;
    envelope.body.correlation_id = message.correlation_id;
    return envelope;
}

inline auto make_service_serialized_message(const MessageView& body, const MessageSerializer& serializer, CorrelationId correlation_id) -> SerializedMessage {
    auto message = SerializedMessage{};
    message.view = body;
    message.view.correlation_id = correlation_id;
    message.serializer.payload_size_hint = serializer.payload_size_hint > 0 ? serializer.payload_size_hint + kServiceEnvelopeHeaderSize : 0;
    message.serializer.encode_binary = [correlation_id, serializer]() -> std::vector<uint8_t> { return encode_service_envelope_binary(correlation_id, serializer); };
    message.serializer.encode_into = [correlation_id, serializer](uint8_t* buf, size_t cap) -> size_t {
        return encode_service_envelope_into(correlation_id, serializer, buf, cap);
    };
    return message;
}

class ShmServiceClient final : public ServiceClient {
  public:
    ShmServiceClient(std::shared_ptr<ShmRoutedMessageBus> bus, std::string service_name, int64_t timeout_ms = kDefaultServiceTimeoutMs)
        : bus_(std::move(bus)), service_name_(std::move(service_name)), timeout_ms_(timeout_ms) {}

    ~ShmServiceClient() override { cancel_all("client destroyed"); }

    auto available() const -> bool override { return bus_ != nullptr && bus_->shm_available(); }

    auto call(const ServiceRequest& request) -> ServiceResponse override {
        auto fut = async_call(request);
        if (fut.wait_for(std::chrono::milliseconds(timeout_ms_)) != std::future_status::ready) {
            return ServiceResponse{false, "timeout waiting for response", {}};
        }
        return fut.get();
    }

    auto call_serialized(const SerializedServiceRequest& request) -> ServiceResponse override {
        auto started = start_call_serialized(request);
        if (started.future.wait_for(std::chrono::milliseconds(timeout_ms_)) != std::future_status::ready) {
            cancel_call(started.correlation_id, "timeout waiting for response");
            return ServiceResponse{false, "timeout waiting for response", {}};
        }
        return started.future.get();
    }

    auto async_call(const ServiceRequest&) -> std::future<ServiceResponse> override {
        auto promise = std::make_shared<std::promise<ServiceResponse>>();
        promise->set_exception(std::make_exception_ptr(star::excep::RuntimeError("ShmServiceBus async_call requires explicit serializer")));
        return promise->get_future();
    }

    auto async_call_serialized(const SerializedServiceRequest& request) -> std::future<ServiceResponse> override {
        return start_call_serialized(request).future;
    }

  private:
    struct StartedCall {
        CorrelationId correlation_id;
        std::future<ServiceResponse> future;
    };

    auto start_call_serialized(const SerializedServiceRequest& request) -> StartedCall {
        if (bus_ == nullptr) {
            auto promise = std::make_shared<std::promise<ServiceResponse>>();
            promise->set_value(ServiceResponse{false, "bus is null", {}});
            return StartedCall{CorrelationId{}, promise->get_future()};
        }

        const auto req_topic = service_name_ + "/request";
        const auto resp_topic = service_name_ + "/response";
        const auto correlation_id = request.request.body.correlation_id.is_valid() ? request.request.body.correlation_id : CorrelationId::generate();

        auto req_msg = make_service_serialized_message(request.request.body, request.serializer, correlation_id);

        auto promise = std::make_shared<std::promise<ServiceResponse>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            if (response_reader_ == nullptr) {
                response_reader_ = bus_->create_reader({resp_topic, "", {}, ChannelOptions{}}, [this](const MessageView& response) { on_response(response); });
                if (!response_reader_) {
                    promise->set_value(ServiceResponse{false, "failed to create response reader", {}});
                    return StartedCall{correlation_id, std::move(future)};
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_[correlation_id] = promise;
        }

        {
            auto req_writer = bus_->create_writer({req_topic, "", {}, ChannelOptions{}});
            const auto r = req_writer->write_serialized(req_msg);
            if (!r.ok()) {
                cancel_call(correlation_id, std::string("request write failed: ") + star::transport::write_status_string(r.status));
            }
        }

        return StartedCall{correlation_id, std::move(future)};
    }

    void on_response(const MessageView& response) {
        const auto envelope = decode_service_envelope(response);
        std::shared_ptr<std::promise<ServiceResponse>> promise;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_.find(envelope.correlation_id);
            if (it == pending_.end()) {
                return;
            }
            promise = std::exchange(it->second, nullptr);
            pending_.erase(it);
        }
        if (promise == nullptr) {
            return;
        }

        promise->set_value(ServiceResponse{true, "", envelope.body});
    }

    void cancel_call(CorrelationId correlation_id, std::string_view reason) {
        std::shared_ptr<std::promise<ServiceResponse>> promise;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_.find(correlation_id);
            if (it != pending_.end()) {
                promise = std::exchange(it->second, nullptr);
                pending_.erase(it);
            }
        }
        if (promise) {
            promise->set_value(ServiceResponse{false, std::string(reason), {}});
        }
    }

    void cancel_all(std::string_view reason) {
        auto ids = std::vector<CorrelationId>{};
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            ids.reserve(pending_.size());
            for (const auto& [id, _] : pending_) {
                ids.emplace_back(id);
            }
        }
        for (const auto& id : ids) {
            cancel_call(id, reason);
        }
    }

    std::shared_ptr<ShmRoutedMessageBus> bus_;
    std::string service_name_;
    int64_t timeout_ms_;
    mutable std::mutex pending_mutex_;
    std::unordered_map<CorrelationId, std::shared_ptr<std::promise<ServiceResponse>>> pending_;
    std::shared_ptr<Reader> response_reader_;
};

class ShmServiceServer final : public ServiceServer {
  public:
    ShmServiceServer(std::shared_ptr<ShmRoutedMessageBus> bus, std::string service_name, ServiceBus::Handler handler)
        : bus_(std::move(bus)), service_name_(std::move(service_name)), handler_(std::move(handler)) {}

    ~ShmServiceServer() override { stop(); }

    void start() override {
        if (running_.exchange(true)) {
            return;
        }
        const auto req_topic = service_name_ + "/request";
        reader_ = bus_->create_reader({req_topic, "", {}, ChannelOptions{}}, [this](const MessageView& msg) { handle_request(msg); });
    }

    void stop() override {
        if (!running_.exchange(false)) {
            return;
        }
        reader_.reset();
    }

  private:
    void handle_request(const MessageView& msg) {
        const auto envelope = decode_service_envelope(msg);
        auto request = ServiceRequest{};
        request.body = envelope.body;
        request.body.correlation_id = envelope.correlation_id;

        SerializedServiceResponse response;
        try {
            response = handler_(request);
        } catch (const std::exception& e) {
            response.response.ok = false;
            response.response.message = e.what();
        } catch (...) {
            response.response.ok = false;
            response.response.message = "handler exception";
        }
        response.response.body.correlation_id = envelope.correlation_id;

        const auto resp_topic = service_name_ + "/response";
        auto resp_msg = make_service_serialized_message(response.response.body, response.serializer, envelope.correlation_id);

        {
            auto resp_writer = bus_->create_writer({resp_topic, "", {}, ChannelOptions{}});
            const auto r = resp_writer->write_serialized(resp_msg);
            if (!r.ok()) {
                logE << "ShmServiceServer response write failed: status=" << star::transport::write_status_string(r.status) << ", err=" << r.error_message;
            }
        }
    }

    std::shared_ptr<ShmRoutedMessageBus> bus_;
    std::string service_name_;
    ServiceBus::Handler handler_;
    std::atomic<bool> running_{false};
    std::shared_ptr<Reader> reader_;
};

class ShmServiceBus final : public ServiceBus {
  public:
    explicit ShmServiceBus(std::shared_ptr<ShmRoutedMessageBus> bus) : bus_(std::move(bus)) {}

    auto create_client(const ServiceDescriptor& descriptor) -> std::shared_ptr<ServiceClient> override {
        return std::make_shared<ShmServiceClient>(bus_, descriptor.name);
    }

    auto create_server(const ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<ServiceServer> override {
        auto server = std::make_shared<ShmServiceServer>(bus_, descriptor.name, std::move(handler));
        server->start();
        return server;
    }

  private:
    std::shared_ptr<ShmRoutedMessageBus> bus_;
};

}  // namespace star::transport

#endif  // __SHM_SERVICE_BUS_H__

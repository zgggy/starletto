#ifndef __SHM_SERVICE_BUS_H__
#define __SHM_SERVICE_BUS_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/transport/api/message_bus.h"
#include "basement/transport/api/service_bus.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace star::transport {

class ShmServiceClient final : public ServiceClient {
  public:
    ShmServiceClient(std::shared_ptr<RoutedMessageBus> bus, std::string service_name) : bus_(std::move(bus)), service_name_(std::move(service_name)) {}

    auto available() const -> bool override { return true; }

    auto call(const ServiceRequest& request) -> ServiceResponse override {
        auto fut = async_call(request);
        if (fut.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
            return {false, "timeout waiting for response", {}, {}, {}, {}};
        }
        return fut.get();
    }

    auto async_call(const ServiceRequest& request) -> std::future<ServiceResponse> override {
        const auto req_topic = service_name_ + "/request";
        const auto resp_topic = service_name_ + "/response";

        auto req_msg = MessageView{};
        req_msg.schema_type_id = request.schema_type_id;
        req_msg.data = request.data_typed;
        req_msg.encode_binary = request.encode_binary;
        req_msg.correlation_id = request.correlation_id.value;

        auto promise = std::make_shared<std::promise<ServiceResponse>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_[request.correlation_id] = promise;
        }

        auto resp_reader = bus_->create_reader({resp_topic, "", {}, ChannelOptions{}}, [this, cid = request.correlation_id](const MessageView& response) { on_response(cid, response); });
        if (!resp_reader) {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_.erase(request.correlation_id);
            promise->set_value({false, "failed to create response reader", {}, {}, {}, {}});
            return future;
        }

        {
            auto req_writer = bus_->create_writer({req_topic, "", {}, ChannelOptions{}});
            req_writer->write(req_msg);
        }

        response_readers_[request.correlation_id] = std::move(resp_reader);
        return future;
    }

  private:
    void on_response(CorrelationId cid, const MessageView& response) {
        std::shared_ptr<std::promise<ServiceResponse>> promise;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            auto it = pending_.find(cid);
            if (it == pending_.end()) {
                return;
            }
            promise = std::exchange(it->second, nullptr);
            pending_.erase(it);
        }
        if (!promise) {
            return;
        }

        auto resp = ServiceResponse{};
        resp.ok = response.correlation_id == 0;
        resp.data_typed = response.data;
        resp.schema_type_id = response.schema_type_id;
        resp.encode_binary = response.encode_binary;
        resp.message = response.correlation_id != 0 ? "error" : "";

        promise->set_value(std::move(resp));
    }

    std::shared_ptr<RoutedMessageBus> bus_;
    std::string service_name_;
    mutable std::mutex pending_mutex_;
    std::unordered_map<CorrelationId, std::shared_ptr<std::promise<ServiceResponse>>> pending_;
    std::unordered_map<CorrelationId, std::shared_ptr<Reader>> response_readers_;
};

class ShmServiceServer final : public ServiceServer {
  public:
    ShmServiceServer(std::shared_ptr<RoutedMessageBus> bus, std::string service_name, ServiceBus::Handler handler)
        : bus_(std::move(bus)), service_name_(std::move(service_name)), handler_(std::move(handler)) {}

    ~ShmServiceServer() override { stop(); }

    void start() {
        if (running_.exchange(true)) {
            return;
        }
        const auto req_topic = service_name_ + "/request";
        reader_ = bus_->create_reader({req_topic, "", {}, ChannelOptions{}}, [this](const MessageView& msg) { handle_request(msg); });
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        reader_.reset();
    }

  private:
    void handle_request(const MessageView& msg) {
        auto request = ServiceRequest{};
        request.data_typed = msg.data;
        request.schema_type_id = msg.schema_type_id;
        request.encode_binary = msg.encode_binary;
        request.correlation_id.value = msg.correlation_id;

        ServiceResponse response;
        try {
            response = handler_(request);
        } catch (...) {
            response = {false, "handler exception", {}, {}, {}, {}};
        }

        response.correlation_id = request.correlation_id;

        const auto resp_topic = service_name_ + "/response";
        auto resp_msg = MessageView{};
        resp_msg.schema_type_id = response.schema_type_id;
        resp_msg.data = response.data_typed;
        resp_msg.encode_binary = response.encode_binary;
        resp_msg.correlation_id = response.ok ? 0 : 1;

        {
            auto resp_writer = bus_->create_writer({resp_topic, "", {}, ChannelOptions{}});
            resp_writer->write(resp_msg);
        }
    }

    std::shared_ptr<RoutedMessageBus> bus_;
    std::string service_name_;
    ServiceBus::Handler handler_;
    std::atomic<bool> running_{false};
    std::shared_ptr<Reader> reader_;
};

class ShmServiceBus final : public ServiceBus {
  public:
    explicit ShmServiceBus(std::shared_ptr<RoutedMessageBus> bus) : bus_(std::move(bus)) {}

    auto create_client(const ServiceDescriptor& descriptor) -> std::shared_ptr<ServiceClient> override { return std::make_shared<ShmServiceClient>(bus_, descriptor.name); }

    auto create_server(const ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<ServiceServer> override {
        return std::make_shared<ShmServiceServer>(bus_, descriptor.name, std::move(handler));
    }

  private:
    std::shared_ptr<RoutedMessageBus> bus_;
};

}  // namespace star::transport

#endif  // __SHM_SERVICE_BUS_H__

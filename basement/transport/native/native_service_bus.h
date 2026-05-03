#ifndef __NATIVE_SERVICE_BUS_H__
#define __NATIVE_SERVICE_BUS_H__

#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "basement/core/exception.h"
#include "basement/transport/api/service_bus.h"

namespace star::transport {

class NativeServiceBus final : public ServiceBus, public std::enable_shared_from_this<NativeServiceBus> {
  public:
    class NativeClient final : public ServiceClient {
      public:
        NativeClient(std::shared_ptr<NativeServiceBus> bus, std::string name) : bus_(std::move(bus)), name_(std::move(name)) {}

        auto available() const -> bool override {
            auto bus = bus_.lock();
            if (not bus) {
                return false;
            }
            return bus->has_service(name_);
        }

        auto call(const ServiceRequest& request) -> ServiceResponse override {
            auto bus = bus_.lock();
            if (not bus) {
                throw star::excep::RuntimeError("NativeServiceBus client call failed, bus expired: ", name_);
            }
            return bus->call_service(name_, request);
        }

        auto call_serialized(const SerializedServiceRequest& request) -> ServiceResponse override {
            auto bus = bus_.lock();
            if (not bus) {
                throw star::excep::RuntimeError("NativeServiceBus client call_serialized failed, bus expired: ", name_);
            }
            return bus->call_service_serialized(name_, request);
        }

        auto async_call(const ServiceRequest& request) -> std::future<ServiceResponse> override {
            auto bus = bus_.lock();
            if (not bus) {
                auto promise = std::make_shared<std::promise<ServiceResponse>>();
                promise->set_exception(std::make_exception_ptr(star::excep::RuntimeError("NativeServiceBus client async_call failed, bus expired: ", name_)));
                return promise->get_future();
            }
            return bus->async_call_service(name_, request);
        }

        auto async_call_serialized(const SerializedServiceRequest& request) -> std::future<ServiceResponse> override {
            auto bus = bus_.lock();
            if (not bus) {
                auto promise = std::make_shared<std::promise<ServiceResponse>>();
                promise->set_exception(std::make_exception_ptr(star::excep::RuntimeError("NativeServiceBus client async_call_serialized failed, bus expired: ", name_)));
                return promise->get_future();
            }
            return bus->async_call_service_serialized(name_, request);
        }

      private:
        std::weak_ptr<NativeServiceBus> bus_;
        std::string name_;
    };

    class NativeServer final : public ServiceServer {
      public:
        NativeServer(std::shared_ptr<NativeServiceBus> bus, std::string name, uint64_t id) : bus_(std::move(bus)), name_(std::move(name)), id_(id) {}

        ~NativeServer() override { stop(); }

        void start() override {}

        void stop() override {
            if (id_ == 0) {
                return;
            }
            auto bus = bus_.lock();
            if (not bus) {
                id_ = 0;
                return;
            }
            bus->erase_service(name_, id_);
            id_ = 0;
        }

      private:
        std::weak_ptr<NativeServiceBus> bus_;
        std::string name_;
        uint64_t id_{0};
    };

    NativeServiceBus() = default;
    ~NativeServiceBus() override = default;

    auto create_client(const ServiceDescriptor& descriptor) -> std::shared_ptr<ServiceClient> override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("NativeServiceBus create_client failed, empty service name");
        }
        return std::make_shared<NativeClient>(this->shared_from_this(), descriptor.name);
    }

    auto create_server(const ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<ServiceServer> override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("NativeServiceBus create_server failed, empty service name");
        }
        if (not handler) {
            throw star::excep::RuntimeError("NativeServiceBus create_server failed, empty handler: ", descriptor.name);
        }

        const auto id = next_server_id_.fetch_add(1, std::memory_order_relaxed);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& entry = services_[descriptor.name];
            if (not entry.handlers.empty()) {
                throw star::excep::RuntimeError("NativeServiceBus create_server failed, duplicate service: ", descriptor.name);
            }
            entry.handlers.emplace(id, std::move(handler));
        }
        auto server = std::make_shared<NativeServer>(this->shared_from_this(), descriptor.name, id);
        server->start();
        return server;
    }

  private:
    struct ServiceEntry {
        std::unordered_map<uint64_t, Handler> handlers;
    };

    auto has_service(const std::string& name) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = services_.find(name);
        return it != services_.end() && not it->second.handlers.empty();
    }

    auto call_service(const std::string& name, const ServiceRequest& request) -> ServiceResponse {
        return call_service_serialized(name, SerializedServiceRequest{request, {}});
    }

    auto call_service_serialized(const std::string& name, const SerializedServiceRequest& request) -> ServiceResponse {
        Handler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = services_.find(name);
            if (it == services_.end() || it->second.handlers.empty()) {
                throw star::excep::RuntimeError("NativeServiceBus call failed, service unavailable: ", name);
            }
            handler = it->second.handlers.begin()->second;
        }
        auto response = handler(request.request).response;
        response.body.correlation_id = request.request.body.correlation_id;
        return response;
    }

    auto async_call_service(const std::string& name, const ServiceRequest& request) -> std::future<ServiceResponse> {
        return async_call_service_serialized(name, SerializedServiceRequest{request, {}});
    }

    auto async_call_service_serialized(const std::string& name, const SerializedServiceRequest& request) -> std::future<ServiceResponse> {
        auto self = this->shared_from_this();
        auto augmented_request = request.request;
        if (not augmented_request.body.correlation_id.is_valid()) {
            augmented_request.body.correlation_id = CorrelationId::generate();
        }

        return std::async(std::launch::async, [self, name, augmented_request]() -> ServiceResponse {
            Handler handler;
            {
                std::lock_guard<std::mutex> lock(self->mutex_);
                const auto it = self->services_.find(name);
                if (it == self->services_.end() || it->second.handlers.empty()) {
                    throw star::excep::RuntimeError("NativeServiceBus async_call failed, service unavailable: ", name);
                }
                handler = it->second.handlers.begin()->second;
            }

            auto response = handler(augmented_request).response;
            response.body.correlation_id = augmented_request.body.correlation_id;
            return response;
        });
    }

    void erase_service(const std::string& name, uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = services_.find(name);
        if (it == services_.end()) {
            return;
        }
        it->second.handlers.erase(id);
        if (it->second.handlers.empty()) {
            services_.erase(it);
        }
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ServiceEntry> services_;
    std::atomic<uint64_t> next_server_id_{1};
};

}  // namespace star::transport

#endif  // __NATIVE_SERVICE_BUS_H__

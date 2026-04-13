#ifndef __STAR_RUNTIME_FACTORY_RUNTIME_FACTORY_H__
#define __STAR_RUNTIME_FACTORY_RUNTIME_FACTORY_H__

#include <atomic>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "basement/core/logger/file_logger.h"
#include "basement/core/logger/logger.h"
#include "basement/core/logger/rolling_file_logger.h"
#include "basement/runtime/context/frame_context.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/runtime/scheduler/native_scheduler.h"
#include "basement/runtime/scheduler/scheduler.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/service_bus.h"
#include "basement/transport/native/native_message_bus.h"
#include "basement/transport/native/native_parameter_store.h"
#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace star::runtime {

class NativeRuntimeContext final : public RuntimeContext {
  public:
    NativeRuntimeContext(std::shared_ptr<star::transport::ChannelRegistry> channel_registry, std::shared_ptr<star::transport::MessageBus> message_bus,
                         std::shared_ptr<star::transport::ServiceBus> service_bus, std::shared_ptr<star::transport::ParameterStore> parameter_store, std::shared_ptr<Scheduler> scheduler,
                         std::shared_ptr<star::schema::SchemaRegistry> schema_registry, star::base::Logger* logger)
        : channel_registry_(std::move(channel_registry)),
          message_bus_(std::move(message_bus)),
          service_bus_(std::move(service_bus)),
          parameter_store_(std::move(parameter_store)),
          scheduler_(std::move(scheduler)),
          schema_registry_(std::move(schema_registry)),
          logger_(logger) {}

    auto channel_registry() -> star::transport::ChannelRegistry& override { return *channel_registry_; }
    auto message_bus() -> star::transport::MessageBus& override { return *message_bus_; }
    auto service_bus() -> star::transport::ServiceBus& override { return *service_bus_; }
    auto parameter_store() -> star::transport::ParameterStore& override { return *parameter_store_; }
    auto scheduler() -> Scheduler& override { return *scheduler_; }
    auto schema_registry() -> star::schema::SchemaRegistry& override { return *schema_registry_; }
    auto logger() -> star::base::Logger& override { return *logger_; }
    auto current_frame() const -> const FrameContext& override { return frame_; }

    void set_current_frame(const FrameContext& frame) { frame_ = frame; }

  private:
    std::shared_ptr<star::transport::ChannelRegistry> channel_registry_;
    std::shared_ptr<star::transport::MessageBus> message_bus_;
    std::shared_ptr<star::transport::ServiceBus> service_bus_;
    std::shared_ptr<star::transport::ParameterStore> parameter_store_;
    std::shared_ptr<Scheduler> scheduler_;
    std::shared_ptr<star::schema::SchemaRegistry> schema_registry_;
    star::base::Logger* logger_{nullptr};
    FrameContext frame_;
};

class NativeServiceBus final : public star::transport::ServiceBus, public std::enable_shared_from_this<NativeServiceBus> {
  public:
    class NativeClient final : public star::transport::ServiceClient {
      public:
        NativeClient(std::shared_ptr<NativeServiceBus> bus, std::string name) : bus_(std::move(bus)), name_(std::move(name)) {}

        auto available() const -> bool override {
            auto bus = bus_.lock();
            if (not bus) {
                return false;
            }
            return bus->has_service(name_);
        }

        auto call(const star::transport::ServiceRequest& request) -> star::transport::ServiceResponse override {
            auto bus = bus_.lock();
            if (not bus) {
                throw star::excep::RuntimeError("NativeServiceBus client call failed, bus expired: ", name_);
            }
            return bus->call_service(name_, request);
        }

        auto async_call(const star::transport::ServiceRequest& request) -> std::future<star::transport::ServiceResponse> override {
            auto bus = bus_.lock();
            if (not bus) {
                auto promise = std::make_shared<std::promise<star::transport::ServiceResponse>>();
                promise->set_exception(std::make_exception_ptr(star::excep::RuntimeError("NativeServiceBus client async_call failed, bus expired: ", name_)));
                return promise->get_future();
            }
            return bus->async_call_service(name_, request);
        }

      private:
        std::weak_ptr<NativeServiceBus> bus_;
        std::string name_;
    };

    class NativeServer final : public star::transport::ServiceServer {
      public:
        NativeServer(std::shared_ptr<NativeServiceBus> bus, std::string name, uint64_t id) : bus_(std::move(bus)), name_(std::move(name)), id_(id) {}

        ~NativeServer() override {
            auto bus = bus_.lock();
            if (not bus) {
                return;
            }
            bus->erase_service(name_, id_);
        }

      private:
        std::weak_ptr<NativeServiceBus> bus_;
        std::string name_;
        uint64_t id_{0};
    };

    NativeServiceBus() = default;
    ~NativeServiceBus() override = default;

    auto create_client(const star::transport::ServiceDescriptor& descriptor) -> std::shared_ptr<star::transport::ServiceClient> override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("NativeServiceBus create_client failed, empty service name");
        }
        return std::make_shared<NativeClient>(this->shared_from_this(), descriptor.name);
    }

    auto create_server(const star::transport::ServiceDescriptor& descriptor, Handler handler) -> std::shared_ptr<star::transport::ServiceServer> override {
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
        return std::make_shared<NativeServer>(this->shared_from_this(), descriptor.name, id);
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

    auto call_service(const std::string& name, const star::transport::ServiceRequest& request) -> star::transport::ServiceResponse {
        Handler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = services_.find(name);
            if (it == services_.end() || it->second.handlers.empty()) {
                throw star::excep::RuntimeError("NativeServiceBus call failed, service unavailable: ", name);
            }
            handler = it->second.handlers.begin()->second;
        }
        auto response = handler(request);
        response.correlation_id = request.correlation_id;
        return response;
    }

    auto async_call_service(const std::string& name, const star::transport::ServiceRequest& request) -> std::future<star::transport::ServiceResponse> {
        auto cid = request.correlation_id.is_valid() ? request.correlation_id : star::transport::CorrelationId::generate();
        auto augmented_request = request;
        augmented_request.correlation_id = cid;

        auto promise = std::make_shared<std::promise<star::transport::ServiceResponse>>();
        auto future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_calls_[cid] = promise;
        }

        try {
            Handler handler;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                const auto it = services_.find(name);
                if (it == services_.end() || it->second.handlers.empty()) {
                    throw star::excep::RuntimeError("NativeServiceBus async_call failed, service unavailable: ", name);
                }
                handler = it->second.handlers.begin()->second;
            }
            auto response = handler(augmented_request);
            response.correlation_id = cid;
            fulfill_pending(cid, response);
        } catch (...) {
            reject_pending(cid, std::current_exception());
        }

        return future;
    }

    void fulfill_pending(star::transport::CorrelationId cid, const star::transport::ServiceResponse& response) {
        std::shared_ptr<std::promise<star::transport::ServiceResponse>> promise;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_calls_.find(cid);
            if (it == pending_calls_.end()) {
                return;
            }
            promise = std::move(it->second);
            pending_calls_.erase(it);
        }
        promise->set_value(response);
    }

    void reject_pending(star::transport::CorrelationId cid, std::exception_ptr ep) {
        std::shared_ptr<std::promise<star::transport::ServiceResponse>> promise;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_calls_.find(cid);
            if (it == pending_calls_.end()) {
                return;
            }
            promise = std::move(it->second);
            pending_calls_.erase(it);
        }
        promise->set_exception(ep);
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
    std::unordered_map<star::transport::CorrelationId, std::shared_ptr<std::promise<star::transport::ServiceResponse>>> pending_calls_;
    std::atomic<uint64_t> next_server_id_{1};
};

class RuntimeFactory {
  public:
    static auto create_native_context(std::string transport_name = {}) -> RuntimeContextPtr {
        auto channel_registry = std::make_shared<star::transport::ChannelRegistry>();
        std::shared_ptr<star::transport::MessageBus> message_bus;
        if (transport_name.empty()) {
            const auto* transport = std::getenv("STAR_TRANSPORT");
            transport_name = transport ? std::string(transport) : std::string();
        }
        if (transport_name == "shm") {
            message_bus = std::make_shared<star::transport::ShmMessageBus>(channel_registry);
        } else if (transport_name == "shm_routed") {
            message_bus = std::make_shared<star::transport::ShmRoutedMessageBus>(channel_registry);
        } else {
            message_bus = std::make_shared<star::transport::NativeMessageBus>(channel_registry);
        }
        auto service_bus = std::make_shared<NativeServiceBus>();
        auto parameter_store = std::make_shared<star::transport::NativeParameterStore>();
        auto scheduler = std::make_shared<star::runtime::NativeScheduler>();
        auto schema_registry = std::make_shared<star::schema::SchemaRegistry>();
        if (const auto* file = std::getenv("STAR_LOG_FILE"); file != nullptr && std::string(file).size() > 0) {
            auto policy = star::base::RollingPolicy{};
            policy.max_file_size = 10 * 1024 * 1024;
            policy.max_backup_files = 5;
            star::base::LoggerManager::get().set_logger(std::make_unique<star::base::RollingFileLogger>(std::string(file), policy));
        } else {
            star::base::LoggerManager::get().set_logger(std::make_unique<star::base::StdoutLogger>());
        }
        auto* logger = star::base::LoggerManager::get().get_logger();
        auto ctx = std::make_shared<NativeRuntimeContext>(std::move(channel_registry), std::move(message_bus), std::move(service_bus), std::move(parameter_store), std::move(scheduler),
                                                          std::move(schema_registry), std::move(logger));
        static_cast<NativeScheduler*>(&ctx->scheduler())->set_context(ctx.get());
        return ctx;
    }
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_FACTORY_RUNTIME_FACTORY_H__

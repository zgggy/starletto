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
#include "basement/transport/native/native_service_bus.h"
#include "basement/runtime/parameter/native_parameter_store.h"
#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace star::runtime {

class NativeRuntimeContext final : public RuntimeContext {
  public:
    NativeRuntimeContext(std::shared_ptr<star::transport::ChannelRegistry> channel_registry, std::shared_ptr<star::transport::MessageBus> message_bus,
                         std::shared_ptr<star::transport::ServiceBus> service_bus, std::shared_ptr<ParameterStore> parameter_store, std::shared_ptr<Scheduler> scheduler,
                         std::shared_ptr<star::schema::SchemaRegistry> schema_registry, std::shared_ptr<star::base::Logger> logger)
        : channel_registry_(std::move(channel_registry)),
          message_bus_(std::move(message_bus)),
          service_bus_(std::move(service_bus)),
          parameter_store_(std::move(parameter_store)),
          scheduler_(std::move(scheduler)),
          schema_registry_(std::move(schema_registry)),
          logger_(std::move(logger)) {}

    auto channel_registry() -> star::transport::ChannelRegistry& override { return *channel_registry_; }
    auto message_bus() -> star::transport::MessageBus& override { return *message_bus_; }
    auto recordable_bus() -> star::transport::RecordableBus* override {
        return dynamic_cast<star::transport::RecordableBus*>(message_bus_.get());
    }
    auto service_bus() -> star::transport::ServiceBus& override { return *service_bus_; }
    auto parameter_store() -> ParameterStore& override { return *parameter_store_; }
    auto scheduler() -> Scheduler& override { return *scheduler_; }
    auto schema_registry() -> star::schema::SchemaRegistry& override { return *schema_registry_; }
    auto logger() -> star::base::Logger& override { return *logger_; }
    auto current_frame() const -> const FrameContext& override { return frame_; }

    void set_current_frame(const FrameContext& frame) override { frame_ = frame; }

  private:
    std::shared_ptr<star::transport::ChannelRegistry> channel_registry_;
    std::shared_ptr<star::transport::MessageBus> message_bus_;
    std::shared_ptr<star::transport::ServiceBus> service_bus_;
    std::shared_ptr<ParameterStore> parameter_store_;
    std::shared_ptr<Scheduler> scheduler_;
    std::shared_ptr<star::schema::SchemaRegistry> schema_registry_;
    std::shared_ptr<star::base::Logger> logger_;
    FrameContext frame_;
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
            try {
                message_bus = std::make_shared<star::transport::ShmMessageBus>(channel_registry);
            } catch (const star::excep::RuntimeError& e) {
                logW << "RuntimeFactory: SHM init failed, falling back to inproc: " << e.what();
                message_bus = std::make_shared<star::transport::NativeMessageBus>(channel_registry);
            }
        } else if (transport_name == "shm_routed") {
            message_bus = std::make_shared<star::transport::ShmRoutedMessageBus>(channel_registry);
        } else {
            message_bus = std::make_shared<star::transport::NativeMessageBus>(channel_registry);
        }
        auto service_bus = std::make_shared<star::transport::NativeServiceBus>();
        auto parameter_store = std::make_shared<NativeParameterStore>();
        auto scheduler = std::make_shared<star::runtime::NativeScheduler>();
        auto schema_registry = std::make_shared<star::schema::SchemaRegistry>();
        if (const auto* file = std::getenv("STAR_LOG_FILE"); file != nullptr && std::string(file).size() > 0) {
            auto policy = star::base::RollingPolicy{};
            policy.max_file_size = 10 * 1024 * 1024;
            policy.max_backup_files = 5;
            star::base::LoggerManager::get().set_logger(std::make_shared<star::base::RollingFileLogger>(std::string(file), policy));
        } else {
            star::base::LoggerManager::get().set_logger(std::make_shared<star::base::StdoutLogger>());
        }
        auto logger = star::base::LoggerManager::get().get_logger_shared();
        auto ctx = std::make_shared<NativeRuntimeContext>(std::move(channel_registry), std::move(message_bus), std::move(service_bus), std::move(parameter_store), std::move(scheduler),
                                                          std::move(schema_registry), std::move(logger));
        static_cast<NativeScheduler*>(&ctx->scheduler())->set_context(ctx.get());
        return ctx;
    }
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_FACTORY_RUNTIME_FACTORY_H__

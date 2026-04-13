#ifndef __STAR_RUNTIME_CONTEXT_RUNTIME_CONTEXT_H__
#define __STAR_RUNTIME_CONTEXT_RUNTIME_CONTEXT_H__

#include <memory>

namespace star::base {
class Logger;
}
namespace star::runtime {
class Scheduler;
class FrameContext;
}  // namespace star::runtime
namespace star::schema {
class SchemaRegistry;
}
namespace star::transport {
class ChannelRegistry;
class MessageBus;
class ServiceBus;
class ParameterStore;
}  // namespace star::transport

namespace star::runtime {

class RuntimeContext {
  public:
    virtual ~RuntimeContext() = default;

    virtual auto message_bus() -> star::transport::MessageBus& = 0;
    virtual auto channel_registry() -> star::transport::ChannelRegistry& = 0;
    virtual auto service_bus() -> star::transport::ServiceBus& = 0;
    virtual auto parameter_store() -> star::transport::ParameterStore& = 0;
    virtual auto scheduler() -> Scheduler& = 0;
    virtual auto schema_registry() -> star::schema::SchemaRegistry& = 0;
    virtual auto logger() -> star::base::Logger& = 0;
    virtual auto current_frame() const -> const FrameContext& = 0;
    virtual void set_current_frame(const FrameContext& frame) = 0;
};

using RuntimeContextPtr = std::shared_ptr<RuntimeContext>;

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_CONTEXT_RUNTIME_CONTEXT_H__

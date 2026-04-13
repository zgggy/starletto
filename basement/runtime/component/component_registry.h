#ifndef __STAR_RUNTIME_COMPONENT_COMPONENT_REGISTRY_H__
#define __STAR_RUNTIME_COMPONENT_COMPONENT_REGISTRY_H__

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "basement/core/exception.h"
#include "basement/runtime/component/component.h"
#include "basement/schema/schema_registry.h"

namespace star::runtime {

class ComponentRegistry {
  public:
    using Factory = std::function<std::unique_ptr<Component>()>;

    void set_schema_registry(star::schema::SchemaRegistry* registry) { schema_registry_ = registry; }

    auto schema_registry() const -> star::schema::SchemaRegistry* { return schema_registry_; }

    void register_factory(const std::string& component_class, Factory factory) {
        if (component_class.empty()) {
            throw star::excep::RuntimeError("ComponentRegistry register_factory failed, empty component_class");
        }
        if (not factory) {
            throw star::excep::RuntimeError("ComponentRegistry register_factory failed, empty factory");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        factories_[component_class] = std::move(factory);
    }

    template <typename _ComponentT>
    void register_component(const std::string& component_class) {
        register_factory(component_class, []() -> std::unique_ptr<Component> { return std::make_unique<_ComponentT>(); });
    }

    auto create(const std::string& component_class) const -> std::unique_ptr<Component> {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = factories_.find(component_class);
        if (it == factories_.end()) {
            throw star::excep::RuntimeError("ComponentRegistry create failed, unknown component_class: ", component_class);
        }
        return it->second();
    }

    auto contains(const std::string& component_class) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return factories_.find(component_class) != factories_.end();
    }

  private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Factory> factories_;
    star::schema::SchemaRegistry* schema_registry_{nullptr};
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_COMPONENT_REGISTRY_H__

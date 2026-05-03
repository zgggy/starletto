#ifndef __STAR_SCHEMA_NATIVE_SCHEMA_BOOTSTRAP_H__
#define __STAR_SCHEMA_NATIVE_SCHEMA_BOOTSTRAP_H__

#include <atomic>

#include "basement/schema/builtin/manifest/component.s.h"
#include "basement/schema/builtin/manifest/module.s.h"
#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/builtin/runtime/process_plan.s.h"
#include "basement/schema/schema_registry.h"

namespace star::schema::native {

inline auto register_builtin_schemas(SchemaRegistry& registry) -> void {
    static std::atomic<bool> registered{false};
    if (registered.exchange(true)) {
        return;
    }
    star::generated::manifest::component::ComponentConf::register_schema(registry);
    star::generated::manifest::module::ModuleConf::register_schema(registry);
    star::generated::runtime::component_load_plan::ComponentLoadPlan::register_schema(registry);
    star::generated::runtime::process_plan::ProcessPlan::register_schema(registry);
}

}  // namespace star::schema::native

#endif  // __STAR_SCHEMA_NATIVE_SCHEMA_BOOTSTRAP_H__

#ifndef __STAR_SCHEMA_NATIVE_SCHEMA_BOOTSTRAP_H__
#define __STAR_SCHEMA_NATIVE_SCHEMA_BOOTSTRAP_H__

#include "basement/schema/builtin/manifest/component.s.h"
#include "basement/schema/builtin/manifest/module.s.h"
#include "basement/schema/builtin/observability/worker_health.s.h"
#include "basement/schema/builtin/observability/worker_stats.s.h"
#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/builtin/runtime/process_plan.s.h"
#include "basement/schema/schema_registry.h"

namespace star::schema::native {

inline auto register_builtin_schemas(SchemaRegistry& registry) -> void {
    // 不设进程级 once 标志：register_schema 本身对同一 registry 幂等，
    // 而进程里可能有多个 RuntimeContext（各自一个 registry），跳过会导致后建的 context 注册不全
    star::generated::manifest::component::ComponentConf::register_schema(registry);
    star::generated::manifest::module::ModuleConf::register_schema(registry);
    star::generated::runtime::component_load_plan::ComponentLoadPlan::register_schema(registry);
    star::generated::runtime::process_plan::ProcessPlan::register_schema(registry);
    star::generated::observability::worker_stats::WorkerStats::register_schema(registry);
    star::generated::observability::worker_health::WorkerHealth::register_schema(registry);
}

}  // namespace star::schema::native

#endif  // __STAR_SCHEMA_NATIVE_SCHEMA_BOOTSTRAP_H__

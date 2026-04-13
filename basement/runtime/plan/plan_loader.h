#ifndef __STAR_RUNTIME_PLAN_PLAN_LOADER_H__
#define __STAR_RUNTIME_PLAN_PLAN_LOADER_H__

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "basement/core/exception.h"
#include "basement/runtime/plan/plan_builder.h"
#include "basement/schema/builtin/manifest/component.s.h"
#include "basement/schema/builtin/manifest/module.s.h"
#include "libraries/common/schema/schema_value.h"

namespace star::runtime::plan {

class PlanLoader {
  public:
    PlanLoader() = default;
    ~PlanLoader() = default;

    auto load_module(const std::string& path) const -> std::optional<ModuleConf> {
        const auto root = star::schema::parse_schema_file(path);
        const auto typed = star::generated::manifest::module::ModuleConf::from_value(root);

        if (typed.module.name.empty()) {
            return std::nullopt;
        }
        return typed;
    }

    auto load_component(const std::string& path) const -> std::optional<ComponentConf> {
        const auto root = star::schema::parse_schema_file(path);
        const auto typed = star::generated::manifest::component::ComponentConf::from_value(root);

        if (typed.component.name.empty() || typed.component.component_class.empty()) {
            return std::nullopt;
        }
        return typed;
    }

    auto load_component_plan(const std::string& path, const LoadContext& ctx) const -> std::optional<ComponentLoadPlan> {
        const auto conf = load_component(path);
        if (not conf) {
            return std::nullopt;
        }
        return build_component_load_plan(*conf, ctx);
    }
};

}  // namespace star::runtime::plan

#endif  // __STAR_RUNTIME_PLAN_PLAN_LOADER_H__

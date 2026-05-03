#ifndef __STAR_RUNTIME_PLAN_PLAN_LOADER_H__
#define __STAR_RUNTIME_PLAN_PLAN_LOADER_H__

#include <exception>
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

template <typename _T>
struct LoadResult {
    std::optional<_T> value;
    std::string error;

    auto ok() const -> bool { return value.has_value(); }
};

class PlanLoader {
  public:
    PlanLoader() = default;
    ~PlanLoader() = default;

    auto load_module(const std::string& path) const -> std::optional<ModuleConf> {
        return try_load_module(path).value;
    }

    auto try_load_module(const std::string& path) const -> LoadResult<ModuleConf> {
        try {
            const auto root = star::schema::parse_schema_file(path);
            const auto typed = star::generated::manifest::module::ModuleConf::from_value(root);
            if (typed.module.name.empty()) {
                throw star::excep::RuntimeError("PlanLoader load_module failed, module.name is empty: path=", path);
            }
            return LoadResult<ModuleConf>{typed, {}};
        } catch (const std::exception& e) {
            return LoadResult<ModuleConf>{std::nullopt, e.what()};
        } catch (...) {
            return LoadResult<ModuleConf>{std::nullopt, "unknown exception"};
        }
    }

    auto load_component(const std::string& path) const -> std::optional<ComponentConf> {
        return try_load_component(path).value;
    }

    auto try_load_component(const std::string& path) const -> LoadResult<ComponentConf> {
        try {
            const auto root = star::schema::parse_schema_file(path);
            const auto typed = star::generated::manifest::component::ComponentConf::from_value(root);
            if (typed.component.name.empty()) {
                throw star::excep::RuntimeError("PlanLoader load_component failed, component.name is empty: path=", path);
            }
            if (typed.component.component_class.empty()) {
                throw star::excep::RuntimeError("PlanLoader load_component failed, component.component_class is empty: path=", path);
            }
            return LoadResult<ComponentConf>{typed, {}};
        } catch (const std::exception& e) {
            return LoadResult<ComponentConf>{std::nullopt, e.what()};
        } catch (...) {
            return LoadResult<ComponentConf>{std::nullopt, "unknown exception"};
        }
    }

    auto load_component_plan(const std::string& path, const LoadContext& ctx) const -> std::optional<ComponentLoadPlan> {
        return try_load_component_plan(path, ctx).value;
    }

    auto try_load_component_plan(const std::string& path, const LoadContext& ctx) const -> LoadResult<ComponentLoadPlan> {
        const auto conf = try_load_component(path);
        if (!conf.value) {
            return LoadResult<ComponentLoadPlan>{std::nullopt, conf.error};
        }
        try {
            return LoadResult<ComponentLoadPlan>{build_component_load_plan(*conf.value, ctx), {}};
        } catch (const std::exception& e) {
            return LoadResult<ComponentLoadPlan>{std::nullopt, e.what()};
        } catch (...) {
            return LoadResult<ComponentLoadPlan>{std::nullopt, "unknown exception"};
        }
    }
};

}  // namespace star::runtime::plan

#endif  // __STAR_RUNTIME_PLAN_PLAN_LOADER_H__

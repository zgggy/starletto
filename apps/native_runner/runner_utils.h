#ifndef __NATIVE_RUNNER_UTILS_H__
#define __NATIVE_RUNNER_UTILS_H__

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "basement/runtime/plan/plan_loader.h"
#include "basement/runtime/process/path_resolver.h"

namespace star::native_runner {

inline auto resolve_load_context(const std::filesystem::path& component_conf_path, const star::runtime::plan::PlanLoader& loader) -> star::runtime::plan::LoadContext {
    auto ctx = star::runtime::plan::LoadContext{};
    ctx.component_conf_path = component_conf_path;

    const auto module_conf = star::runtime::process::find_nearest_module_conf(component_conf_path);
    if (not module_conf) {
        return ctx;
    }

    ctx.module_conf_path = *module_conf;
    ctx.module_dir = module_conf->parent_path();

    const auto module_conf_data = loader.try_load_module(module_conf->string());
    if (not module_conf_data.value) {
        logE << "load module.conf failed: path=" << module_conf->string() << ", error=" << module_conf_data.error;
        return ctx;
    }

    if (not module_conf_data.value->module.name.empty()) {
        ctx.module_name = module_conf_data.value->module.name;
    }

    auto config_dir = module_conf->parent_path() / (module_conf_data.value->layout.has_value() && module_conf_data.value->layout->config_dir.has_value() ? *module_conf_data.value->layout->config_dir : ".");
    ctx.module_config_dir = std::filesystem::absolute(config_dir);

    return ctx;
}

static auto build_component_load_plan(const std::filesystem::path& component_conf_path, const star::runtime::plan::PlanLoader& loader) -> std::optional<star::runtime::plan::ComponentLoadPlan> {
    const auto ctx = resolve_load_context(component_conf_path, loader);
    const auto result = loader.try_load_component_plan(component_conf_path.string(), ctx);
    if (not result.value) {
        logE << "build component load plan failed: path=" << component_conf_path.string() << ", error=" << result.error;
    }
    return result.value;
}

static auto normalize_inputs_to_component_conf_paths(const std::vector<std::string>& raw_paths) -> std::vector<std::string> {
    star::runtime::plan::PlanLoader loader;
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;

    for (const auto& p : raw_paths) {
        const auto path = std::filesystem::path(p);
        if (std::filesystem::is_directory(path)) {
            auto found = star::runtime::process::expand_component_confs_from_dir(path);
            for (auto& f : found) {
                if (seen.emplace(f).second) {
                    out.emplace_back(std::move(f));
                }
            }
            continue;
        }
        if (path.filename() == "module.conf") {
            auto module_order = star::runtime::process::collect_module_conf_order(path, loader);
            for (const auto& module_conf : module_order) {
                auto found = star::runtime::process::collect_component_confs_from_module_conf(module_conf, loader);
                if (found.empty()) {
                    logE << "module.conf resolved no component.conf: " << module_conf.string();
                }
                for (auto& f : found) {
                    if (seen.emplace(f).second) {
                        out.emplace_back(std::move(f));
                    }
                }
            }
            continue;
        }
        if (seen.emplace(p).second) {
            out.emplace_back(p);
        }
    }
    return out;
}

}  // namespace star::native_runner

#endif  // __NATIVE_RUNNER_UTILS_H__

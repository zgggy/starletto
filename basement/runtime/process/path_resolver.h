#ifndef __STAR_RUNTIME_PROCESS_PATH_RESOLVER_H__
#define __STAR_RUNTIME_PROCESS_PATH_RESOLVER_H__

#include <algorithm>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "basement/core/logger/logger.h"
#include "basement/runtime/plan/plan_loader.h"

namespace star::runtime::process {

inline auto find_project_root(std::filesystem::path start) -> std::optional<std::filesystem::path> {
    if (start.empty()) {
        return std::nullopt;
    }
    if (std::filesystem::is_regular_file(start)) {
        start = start.parent_path();
    }
    start = std::filesystem::absolute(start);
    for (auto cur = start; not cur.empty(); cur = cur.parent_path()) {
        const auto basement_dir = cur / "basement";
        const auto bin_dir = cur / "bin";
        const auto modules_dir = cur / "modules";
        if (std::filesystem::is_directory(modules_dir) && (std::filesystem::is_directory(basement_dir) || std::filesystem::is_directory(bin_dir))) {
            return cur;
        }
        if (cur == cur.root_path()) {
            break;
        }
    }
    return std::nullopt;
}

inline auto expand_component_confs_from_dir(const std::filesystem::path& dir) -> std::vector<std::string> {
    std::vector<std::string> out;
    if (not std::filesystem::is_directory(dir)) {
        return out;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (not entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename() == "component.conf") {
            out.push_back(std::filesystem::weakly_canonical(entry.path()).string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline auto expand_module_confs_from_dir(const std::filesystem::path& dir) -> std::vector<std::string> {
    std::vector<std::string> out;
    if (not std::filesystem::is_directory(dir)) {
        return out;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (not entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename() == "module.conf") {
            out.push_back(std::filesystem::weakly_canonical(entry.path()).string());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline auto find_nearest_module_conf(const std::filesystem::path& component_conf) -> std::optional<std::filesystem::path> {
    auto dir = component_conf.parent_path();
    while (not dir.empty()) {
        const auto candidate = dir / "module.conf";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (dir == dir.root_path()) {
            break;
        }
        dir = dir.parent_path();
    }
    return std::nullopt;
}

inline auto collect_component_confs_from_module_conf(const std::filesystem::path& module_conf_path, const star::runtime::plan::PlanLoader& loader) -> std::vector<std::string> {
    const auto module_conf = loader.try_load_module(module_conf_path.string());
    if (not module_conf.value) {
        logE << "load module.conf failed: path=" << module_conf_path.string() << ", error=" << module_conf.error;
        return {};
    }

    if (module_conf.value->components.has_value() && not module_conf.value->components->empty()) {
        std::vector<std::string> out;
        out.reserve(module_conf.value->components->size());
        const auto base = module_conf_path.parent_path();
        for (const auto& rel : *module_conf.value->components) {
            auto p = std::filesystem::path(rel);
            if (not p.is_absolute()) {
                p = base / p;
            }
            p = std::filesystem::weakly_canonical(p);
            if (not std::filesystem::exists(p)) {
                logE << "module.conf listed component.conf not exists: module_conf=" << module_conf_path.string() << ", component_conf=" << rel << ", resolved=" << p.string();
                continue;
            }
            out.emplace_back(p.string());
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    auto base = module_conf_path.parent_path();
    auto component_dir = base / (module_conf.value->layout.has_value() && module_conf.value->layout->component_dir.has_value() ? *module_conf.value->layout->component_dir : ".");
    auto out = expand_component_confs_from_dir(component_dir);
    const auto config_dir_value = module_conf.value->layout.has_value() && module_conf.value->layout->config_dir.has_value() ? *module_conf.value->layout->config_dir : ".";
    const auto config_dir = std::filesystem::weakly_canonical(base / config_dir_value);
    out.erase(std::remove_if(out.begin(), out.end(),
                             [&config_dir](const std::string& path) {
                                 const auto file_path = std::filesystem::weakly_canonical(path);
                                 const auto config_dir_string = config_dir.string();
                                 const auto file_path_string = file_path.string();
                                 if (config_dir_string.empty() || file_path_string.size() < config_dir_string.size()) {
                                     return false;
                                 }
                                 if (file_path_string.compare(0, config_dir_string.size(), config_dir_string) != 0) {
                                     return false;
                                 }
                                 return file_path_string.size() == config_dir_string.size() || file_path_string[config_dir_string.size()] == std::filesystem::path::preferred_separator;
                             }),
              out.end());
    return out;
}

inline auto collect_module_conf_order(const std::filesystem::path& module_conf_path, const star::runtime::plan::PlanLoader& loader) -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> order;
    std::unordered_set<std::string> visited;

    const auto root = find_project_root(module_conf_path);
    const auto resolve_dep = [&](const std::string& module_name) -> std::optional<std::filesystem::path> {
        if (not root) {
            return std::nullopt;
        }
        auto p = *root / "modules" / module_name / "module.conf";
        if (std::filesystem::exists(p)) {
            return p;
        }
        return std::nullopt;
    };

    const std::function<void(const std::filesystem::path&)> dfs = [&](const std::filesystem::path& p) {
        const auto key = std::filesystem::absolute(p).string();
        if (visited.find(key) != visited.end()) {
            return;
        }
        visited.insert(key);
        const auto module_conf = loader.try_load_module(p.string());
        if (module_conf.value) {
            const auto& deps =
                module_conf.value->dependency.has_value() && module_conf.value->dependency->modules.has_value() ? *module_conf.value->dependency->modules : std::vector<std::string>{};
            for (const auto& dep : deps) {
                if (auto dep_path = resolve_dep(dep)) {
                    dfs(*dep_path);
                }
            }
        } else {
            logE << "load module.conf failed: path=" << p.string() << ", error=" << module_conf.error;
        }
        order.push_back(p);
    };

    dfs(module_conf_path);
    return order;
}

inline auto normalize_inputs_to_module_conf_paths(const std::vector<std::string>& raw_paths) -> std::vector<std::string> {
    std::unordered_set<std::string> seen;
    std::vector<std::string> out;

    [[maybe_unused]] star::runtime::plan::PlanLoader loader;
    const auto cwd_root = find_project_root(std::filesystem::current_path());

    const auto add_module_conf = [&](const std::filesystem::path& p) {
        auto abs = std::filesystem::weakly_canonical(p).string();
        if (seen.emplace(abs).second) {
            out.emplace_back(std::move(abs));
        }
    };

    if (raw_paths.empty()) {
        if (cwd_root) {
            for (const auto& f : expand_module_confs_from_dir(*cwd_root / "modules")) {
                add_module_conf(f);
            }
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    for (const auto& p : raw_paths) {
        const auto path = std::filesystem::path(p);
        if (std::filesystem::is_directory(path)) {
            for (const auto& f : expand_module_confs_from_dir(path)) {
                add_module_conf(f);
            }
            continue;
        }
        if (std::filesystem::exists(path) && path.filename() == "module.conf") {
            add_module_conf(path);
            continue;
        }
        if (std::filesystem::exists(path) && path.filename() == "component.conf") {
            if (const auto module_conf = find_nearest_module_conf(path)) {
                add_module_conf(*module_conf);
            }
            continue;
        }
        if (cwd_root) {
            const auto candidate = *cwd_root / "modules" / p / "module.conf";
            if (std::filesystem::exists(candidate)) {
                add_module_conf(candidate);
                continue;
            }
        }
    }

    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

}  // namespace star::runtime::process

#endif  // __STAR_RUNTIME_PROCESS_PATH_RESOLVER_H__

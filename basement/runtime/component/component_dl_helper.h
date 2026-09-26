#ifndef __COMPONENT_DL_HELPER_H__
#define __COMPONENT_DL_HELPER_H__

#include <dlfcn.h>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "basement/core/exception.h"
#include "basement/runtime/plan/plan_builder.h"

namespace star::runtime {

class ComponentRegistry;

struct DlHandle {
    void* handle{nullptr};

    DlHandle() = default;
    explicit DlHandle(void* h) : handle(h) {}

    DlHandle(const DlHandle&) = delete;
    auto operator=(const DlHandle&) -> DlHandle& = delete;

    DlHandle(DlHandle&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
    auto operator=(DlHandle&& other) noexcept -> DlHandle& {
        if (this == &other) {
            return *this;
        }
        if (handle != nullptr) {
            dlclose(handle);
        }
        handle = std::exchange(other.handle, nullptr);
        return *this;
    }

    ~DlHandle() {
        if (handle != nullptr) {
            dlclose(handle);
        }
    }
};

class ComponentDlHelper {
  public:
    auto resolve_library_key(const star::runtime::plan::ComponentLoadPlan& plan) const -> std::string {
        const auto& lib = plan.component_library.value_or("");
        if (lib.empty()) {
            return {};
        }
        if (lib.find('/') != std::string::npos || lib.find('\\') != std::string::npos) {
            const auto& module_dir = plan.module_dir.value_or("");
            if (not module_dir.empty() && lib.size() > 0 && lib.front() != '/') {
                return module_dir + "/" + lib;
            }
            return lib;
        }
        if (auto resolved = resolve_component_dir_library(plan, lib)) {
            return *resolved;
        }
        if (auto resolved = resolve_module_dir_library(plan, lib)) {
            return *resolved;
        }
        if (auto resolved = resolve_build_tree_library(plan, lib)) {
            return *resolved;
        }
        if (auto resolved = resolve_env_library(lib)) {
            return *resolved;
        }
        return lib;
    }

    // 插件入口约定：extern "C" void star_register_components(ComponentRegistry* registry)
    using RegisterComponentsFn = void (*)(star::runtime::ComponentRegistry*);

    void load_dynamic_library(const star::runtime::plan::ComponentLoadPlan& plan, std::unordered_map<std::string, DlHandle>& handles,
                              star::runtime::ComponentRegistry* registry) {
        const auto library_key = resolve_library_key(plan);
        auto it = handles.find(library_key);
        if (it == handles.end()) {
            void* handle = dlopen(library_key.c_str(), RTLD_NOW | RTLD_LOCAL);
            if (handle == nullptr) {
                const auto* err = dlerror();
                throw star::excep::RuntimeError("NativeComponentHost dlopen failed: library=", library_key, ", component_class=", plan.component_class, ", err=", (err ? err : "unknown"));
            }
            it = handles.emplace(library_key, DlHandle(handle)).first;
        }

        dlerror();
        auto sym = dlsym(it->second.handle, "star_register_components");
        const auto* err = dlerror();
        if (err != nullptr || sym == nullptr) {
            throw star::excep::RuntimeError("NativeComponentHost dlsym failed: library=", library_key, ", symbol=star_register_components", ", component_class=", plan.component_class,
                                            ", err=", (err ? err : "unknown"));
        }
        if (registry == nullptr) {
            throw star::excep::RuntimeError("NativeComponentHost register failed, registry is null: library=", library_key, ", component_class=", plan.component_class);
        }
        // 必须把真实的 registry 交给插件：早先把 register_fn 这个函数指针当成了 registry，
        // 插件拿到垃圾地址后对其中的 mutex 加锁，触发「mutex lock failed: Invalid argument」
        auto fn = reinterpret_cast<RegisterComponentsFn>(sym);
        fn(registry);
    }

  private:
    static auto resolve_component_dir_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string> {
        const auto& conf_path = plan.component_conf_path.value_or("");
        if (conf_path.empty() || lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        const auto cp = std::filesystem::path(conf_path);
        auto candidate = std::filesystem::weakly_canonical(cp.parent_path() / lib, ec);
        if (ec) {
            return std::nullopt;
        }
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        return std::nullopt;
    }

    static auto resolve_module_dir_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string> {
        const auto& module_dir = plan.module_dir.value_or("");
        if (module_dir.empty() || lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        auto candidate = std::filesystem::weakly_canonical(std::filesystem::path(module_dir) / lib, ec);
        if (ec) {
            return std::nullopt;
        }
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        return std::nullopt;
    }

    static auto split_path_list(const char* value) -> std::vector<std::string> {
        if (value == nullptr) {
            return {};
        }
        auto s = std::string(value);
        std::vector<std::string> out;
        std::string cur;
        for (const auto ch : s) {
            if (ch == ':') {
                if (not cur.empty()) {
                    out.emplace_back(std::move(cur));
                    cur = {};
                }
                continue;
            }
            cur.push_back(ch);
        }
        if (not cur.empty()) {
            out.emplace_back(std::move(cur));
        }
        return out;
    }

    static auto resolve_env_library(const std::string& lib) -> std::optional<std::string> {
        if (lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        const auto search_paths = split_path_list(std::getenv("STAR_LIB_PATH"));
        for (const auto& dir : search_paths) {
            if (dir.empty()) {
                continue;
            }
            const auto base = std::filesystem::path(dir);
            const auto candidate = std::filesystem::weakly_canonical(base / lib, ec);
            if (ec) {
                continue;
            }
            if (std::filesystem::exists(candidate, ec) && !ec) {
                return candidate.string();
            }
        }
        return std::nullopt;
    }

    static auto resolve_build_tree_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string> {
        const auto& conf_path = plan.component_conf_path.value_or("");
        if (conf_path.empty() || lib.empty()) {
            return std::nullopt;
        }

        std::error_code ec;
        const auto cp = std::filesystem::path(conf_path);
        auto root = find_project_root(cp);
        if (not root) {
            return std::nullopt;
        }

        auto component_dir = cp.parent_path();
        component_dir = std::filesystem::weakly_canonical(component_dir, ec);
        if (ec) {
            return std::nullopt;
        }

        const auto rel = std::filesystem::relative(component_dir, *root, ec);
        if (ec) {
            return std::nullopt;
        }

        auto candidate = std::filesystem::weakly_canonical((*root / "build" / rel / lib), ec);
        if (ec) {
            return std::nullopt;
        }
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        return std::nullopt;
    }

    static auto find_project_root(std::filesystem::path start) -> std::optional<std::filesystem::path> {
        if (start.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        if (std::filesystem::is_regular_file(start, ec)) {
            start = start.parent_path();
        }
        start = std::filesystem::absolute(start, ec);
        if (ec) {
            return std::nullopt;
        }
        for (auto cur = start; !cur.empty(); cur = cur.parent_path()) {
            const auto basement_dir = cur / "basement";
            const auto modules_dir = cur / "modules";
            if (std::filesystem::is_directory(basement_dir, ec) && std::filesystem::is_directory(modules_dir, ec) && !ec) {
                return cur;
            }
            if (cur == cur.root_path()) {
                break;
            }
        }
        return std::nullopt;
    }
};

}  // namespace star::runtime

#endif  // __COMPONENT_DL_HELPER_H__

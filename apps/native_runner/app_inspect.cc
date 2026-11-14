#include "apps/native_runner/app.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "apps/native_runner/runner_utils.h"
#include "basement/core/build_info.h"
#include "basement/plugin/star_plugin_abi.h"
#include "basement/runtime/component/native_component_host.h"
#include "basement/runtime/factory/runtime_factory.h"
#include "basement/runtime/observability/resource_monitor.h"
#include "basement/schema/builtin/observability/worker_health.s.h"
#include "basement/schema/builtin/observability/worker_stats.s.h"
#include "basement/runtime/plan/plan_diagnostics.h"
#include "basement/runtime/plan/plan_loader.h"
#include "basement/runtime/process/path_resolver.h"
#include "basement/runtime/process/process_orchestrator.h"
#include "basement/schema/native/schema_bootstrap.h"
#include "basement/schema/schema_value.h"
#include "basement/transport/shm/shm_channel.h"
#include "basement/transport/shm/shm_routed_message_bus.h"
#include "basement/transport/rpc/rpc.h"
#include "basement/transport/shm/shm_views.h"


namespace star::native_runner {

    auto App::list(const std::vector<std::string>& module_conf_paths) -> int  {
        star::runtime::plan::PlanLoader loader;
        for (const auto& module_conf : module_conf_paths) {
            const auto module_conf_data = loader.load_module(module_conf);
            if (not module_conf_data) {
                logE << "list failed, load module conf failed: " << module_conf;
                continue;
            }
            std::cout << "module " << module_conf_data->module.name << " " << module_conf << "\n";
            auto component_confs = star::runtime::process::collect_component_confs_from_module_conf(std::filesystem::path(module_conf), loader);
            for (const auto& component_conf : component_confs) {
                const auto component_plan = build_component_load_plan(component_conf, loader);
                if (not component_plan) {
                    std::cout << "component <invalid> conf=" << component_conf << "\n";
                    continue;
                }
                std::cout << "component " << component_plan->module_name << "/" << component_plan->component_name << " class=" << component_plan->component_class
                          << " lib=" << component_plan->component_library.value_or("") << " conf=" << component_conf << "\n";
            }
        }
        return 0;
    }

    auto App::graph_modules_dot(const std::vector<std::string>& module_conf_paths) -> int  {
        star::runtime::plan::PlanLoader loader;
        std::unordered_map<std::string, std::vector<std::string>> deps;
        for (const auto& module_conf : module_conf_paths) {
            const auto module_conf_data = loader.load_module(module_conf);
            if (not module_conf_data) {
                continue;
            }
            deps[module_conf_data->module.name] =
                module_conf_data->dependency.has_value() && module_conf_data->dependency->modules.has_value() ? *module_conf_data->dependency->modules : std::vector<std::string>{};
        }
        std::cout << "digraph module_deps {\n";
        for (const auto& [name, ds] : deps) {
            std::cout << "  \"" << name << "\";\n";
            for (const auto& d : ds) {
                std::cout << "  \"" << name << "\" -> \"" << d << "\";\n";
            }
        }
        std::cout << "}\n";
        return 0;
    }

    auto App::graph_components_dot(const std::vector<std::string>& module_conf_paths) -> int  {
        star::runtime::plan::PlanLoader loader;
        std::vector<star::runtime::plan::ComponentLoadPlan> components;

        std::unordered_set<std::string> visited;
        for (const auto& module_conf : module_conf_paths) {
            auto module_order = star::runtime::process::collect_module_conf_order(std::filesystem::path(module_conf), loader);
            for (const auto& m : module_order) {
                const auto key = std::filesystem::absolute(m).string();
                if (not visited.emplace(key).second) {
                    continue;
                }
                auto component_confs = star::runtime::process::collect_component_confs_from_module_conf(m, loader);
                for (const auto& component_conf : component_confs) {
                    const auto component_plan = build_component_load_plan(component_conf, loader);
                    if (not component_plan) {
                        continue;
                    }
                    components.push_back(std::move(*component_plan));
                }
            }
        }

        std::cout << "digraph component_topology {\n";
        std::cout << "  rankdir=LR;\n";

        auto escape = [](const std::string& s) -> std::string {
            std::string out;
            out.reserve(s.size());
            for (const auto ch : s) {
                if (ch == '"') {
                    out.push_back('\\');
                }
                out.push_back(ch);
            }
            return out;
        };

        for (const auto& c : components) {
            const auto id = c.module_name + "/" + c.component_name;
            std::cout << "  \"" << escape(id) << "\" [shape=box];\n";
        }

        std::unordered_set<std::string> channels;
        for (const auto& c : components) {
            for (const auto& e : c.outputs.value_or(std::vector<star::runtime::plan::PortPlan>{})) {
                if (e.channel.has_value()) {
                    channels.emplace(*e.channel);
                }
            }
            for (const auto& e : c.inputs.value_or(std::vector<star::runtime::plan::PortPlan>{})) {
                if (e.channel.has_value()) {
                    channels.emplace(*e.channel);
                }
            }
        }
        for (const auto& t : channels) {
            std::cout << "  \"" << escape(t) << "\" [shape=ellipse];\n";
        }

        for (const auto& c : components) {
            const auto id = c.module_name + "/" + c.component_name;
            for (const auto& e : c.outputs.value_or(std::vector<star::runtime::plan::PortPlan>{})) {
                if (e.channel.has_value()) {
                    std::cout << "  \"" << escape(id) << "\" -> \"" << escape(*e.channel) << "\";\n";
                }
            }
            for (const auto& e : c.inputs.value_or(std::vector<star::runtime::plan::PortPlan>{})) {
                if (e.channel.has_value()) {
                    std::cout << "  \"" << escape(*e.channel) << "\" -> \"" << escape(id) << "\";\n";
                }
            }
        }

        std::cout << "}\n";
        return 0;
    }

    auto App::doctor(const std::vector<std::string>& module_conf_paths, bool deep) -> int  {
        star::runtime::plan::PlanLoader loader;
        bool ok = true;
        auto all_plans = std::vector<star::runtime::plan::ComponentLoadPlan>{};
        for (const auto& module_conf : module_conf_paths) {
            const auto module_conf_data = loader.load_module(module_conf);
            if (not module_conf_data) {
                logE << "doctor: load module conf failed: " << module_conf;
                ok = false;
                continue;
            }
            auto component_confs = star::runtime::process::collect_component_confs_from_module_conf(std::filesystem::path(module_conf), loader);
            if (component_confs.empty()) {
                logE << "doctor: module has no components: " << module_conf;
                ok = false;
                continue;
            }
            for (const auto& component_conf : component_confs) {
                const auto component_plan = build_component_load_plan(component_conf, loader);
                if (not component_plan) {
                    logE << "doctor: build component load plan failed: " << component_conf;
                    ok = false;
                    continue;
                }
                all_plans.push_back(*component_plan);
                if (component_plan->component_class.empty()) {
                    logE << "doctor: empty component_class: " << component_conf;
                    ok = false;
                }
                if (!component_plan->static_link.value_or(false) && component_plan->component_library.value_or("").empty()) {
                    logE << "doctor: empty component_library but static_link=false: " << component_conf;
                    ok = false;
                }
                if (!component_plan->trigger.has_value() || (component_plan->trigger->type.value_or("").empty() && component_plan->trigger->cycle_ms.value_or(0) <= 0 &&
                                                             component_plan->trigger->readers.value_or(std::vector<std::string>{}).empty())) {
                    logE << "doctor: component has no trigger: " << component_conf;
                    ok = false;
                }

                if (deep && !component_plan->static_link.value_or(false) && !component_plan->component_library.value_or("").empty()) {
                    auto lib_key = resolve_library_key(*component_plan);
                    void* handle = dlopen(lib_key.c_str(), RTLD_NOW | RTLD_LOCAL);
                    if (handle == nullptr) {
                        const auto* err = dlerror();
                        logE << "doctor: dlopen failed: lib=" << lib_key << ", conf=" << component_conf << ", err=" << (err ? err : "unknown");
                        ok = false;
                    } else {
                        dlerror();
                        auto* cabi_sym = dlsym(handle, STAR_PLUGIN_ENTRY_SYMBOL);
                        if (cabi_sym != nullptr) {
                            // C ABI 插件：纯 C 编译，不要求工具链一致；校验 ABI 版本与 descriptor
                            const auto* plugin = reinterpret_cast<star_plugin_entry_v1_fn>(cabi_sym)();
                            if (plugin == nullptr) {
                                logE << "doctor: C ABI plugin entry returned null: lib=" << lib_key;
                                ok = false;
                            } else if (plugin->abi_version != STAR_PLUGIN_ABI_VERSION) {
                                logE << "doctor: C ABI plugin version mismatch: lib=" << lib_key << ", plugin=" << plugin->abi_version
                                     << ", host=" << STAR_PLUGIN_ABI_VERSION;
                                ok = false;
                            } else if (plugin->component_count == 0 || plugin->components == nullptr) {
                                logE << "doctor: C ABI plugin registers no components: lib=" << lib_key;
                                ok = false;
                            } else {
                                logI << "doctor: C ABI plugin v" << plugin->abi_version << ": lib=" << lib_key
                                     << ", name=" << (plugin->plugin_name ? plugin->plugin_name : "<unnamed>")
                                     << ", components=" << plugin->component_count;
                            }
                        } else {
                            dlerror();
                            auto sym = dlsym(handle, "star_register_components");
                            const auto* err = dlerror();
                            if (err != nullptr || sym == nullptr) {
                                logE << "doctor: dlsym failed: lib=" << lib_key << ", symbols=star_plugin_entry_v1|star_register_components"
                                     << ", conf=" << component_conf << ", err=" << (err ? err : "unknown");
                                ok = false;
                            }
                            // 插件 ABI 契约：header-only 运行时要求插件与宿主同工具链
                            auto fingerprint_sym = dlsym(handle, "star_toolchain_fingerprint");
                            if (fingerprint_sym == nullptr) {
                                logW << "doctor: plugin has no toolchain fingerprint (unversioned), ABI not verified: " << lib_key;
                            } else {
                                const auto* plugin_fingerprint = reinterpret_cast<const char* (*)()>(fingerprint_sym)();
                                const auto* host_fingerprint = star::base::toolchain_fingerprint();
                                if (plugin_fingerprint == nullptr || std::string(plugin_fingerprint) != std::string(host_fingerprint)) {
                                    logE << "doctor: plugin toolchain mismatch: lib=" << lib_key
                                         << ", plugin=" << (plugin_fingerprint ? plugin_fingerprint : "null")
                                         << ", host=" << host_fingerprint;
                                    ok = false;
                                }
                            }
                        }
                        dlclose(handle);
                    }
                }
            }
        }
        for (const auto& issue : star::runtime::plan::find_channel_type_conflicts(all_plans)) {
            logE << "doctor: " << issue;
            ok = false;
        }
        return ok ? 0 : 1;
    }

}  // namespace star::native_runner

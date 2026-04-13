#ifndef __NATIVE_RUNNER_APP_H__
#define __NATIVE_RUNNER_APP_H__

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "apps/native_runner/runner_utils.h"
#include "apps/native_runner/types.h"
#include "basement/runtime/plan/plan_loader.h"
#include "basement/runtime/plan/plan_builder.h"
#include "basement/runtime/component/native_component_host.h"
#include "basement/runtime/observability/resource_monitor.h"
#include "basement/runtime/process/path_resolver.h"
#include "basement/schema/native/schema_bootstrap.h"
#include "basement/transport/shm/shm_routed_message_bus.h"
#include "libraries/common/schema/schema_value.h"
#include "modules/control/pid/control_component.h"
#include "modules/planning/motion/motion_component.h"

namespace star::native_runner {

class App {
  public:
    auto run(const RunOptions& options, const std::vector<std::string>& component_conf_paths) -> int {
        if (component_conf_paths.empty()) {
            logE << "no component conf provided";
            return 2;
        }

        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport_name);
        configure_routed_transport_if_needed(*context, options);
        register_generated_schemas(context->schema_registry());
        star::schema::native::register_business_schemas(context->schema_registry());
        auto* message_bus = &context->message_bus();
        auto registry = std::make_shared<star::runtime::ComponentRegistry>();
        registry->set_schema_registry(&context->schema_registry());
        star::planning::register_native_components(*registry);
        star::control::register_control_components(*registry);

        star::runtime::plan::PlanLoader loader;
        star::runtime::NativeComponentHost host(std::move(context), std::move(registry));

        std::vector<star::runtime::plan::ComponentLoadPlan> component_plans;
        int64_t min_cycle_ms = 0;
        for (const auto& p : component_conf_paths) {
            const auto plan = build_component_load_plan(p, loader);
            if (not plan) {
                logE << "build component load plan failed: " << p;
                return 3;
            }
            component_plans.push_back(*plan);
            host.load(*plan);
            logI << "loaded component: " << plan->component_name << ", class=" << plan->component_class;
            if (plan->trigger.has_value() && plan->trigger->cycle_ms.value_or(0) > 0) {
                const auto cycle_ms = plan->trigger->cycle_ms.value_or(0);
                if (min_cycle_ms <= 0 || cycle_ms < min_cycle_ms) {
                    min_cycle_ms = cycle_ms;
                }
            }
        }

        const auto process_plan = star::runtime::plan::build_process_plan(component_plans);
        host.set_process_plan(process_plan);

        auto sleep_ms = options.sleep_ms;
        if (sleep_ms <= 0) {
            sleep_ms = min_cycle_ms;
        }

        host.configure();
        host.start();
        auto timeout_ms = int64_t{0};
        if (sleep_ms > 0 && options.frames > 0) {
            timeout_ms = sleep_ms * options.frames + 500;
        } else if (sleep_ms > 0) {
            timeout_ms = sleep_ms + 500;
        } else {
            timeout_ms = 5000;
        }
        const auto ok = host.wait_for_frames(options.frames, timeout_ms);
        host.stop();
        maybe_dump_transport_stats(*message_bus);

        return ok ? 0 : 10;
    }

    auto run_multi_process(const RunOptions& options, const std::vector<std::string>& component_conf_paths) -> int {
        star::runtime::plan::PlanLoader loader;
        std::unordered_map<std::string, std::vector<std::string>> groups;
        std::vector<star::runtime::plan::ComponentLoadPlan> component_plans;
        for (const auto& p : component_conf_paths) {
            const auto plan = build_component_load_plan(p, loader);
            if (not plan) {
                logE << "multi-process build component load plan failed: " << p;
                return 3;
            }
            component_plans.push_back(std::move(*plan));
        }

        const auto process_plan = star::runtime::plan::build_process_plan(component_plans);
        auto channel_producers = std::unordered_map<std::string, std::string>{};
        auto channel_consumers = std::unordered_map<std::string, std::unordered_set<std::string>>{};
        if (process_plan.groups.has_value()) {
            for (const auto& group_plan : *process_plan.groups) {
                groups[group_plan.name] = group_plan.component_conf_paths.value_or(std::vector<std::string>{});
                for (const auto& channel : group_plan.produced_channels.value_or(std::vector<std::string>{})) {
                    if (channel.empty()) {
                        continue;
                    }
                    if (channel_producers.find(channel) == channel_producers.end()) {
                        channel_producers.emplace(channel, group_plan.name);
                    }
                }
            }
            for (const auto& group_plan : *process_plan.groups) {
                for (const auto& channel : group_plan.consumed_channels.value_or(std::vector<std::string>{})) {
                    if (channel.empty()) {
                        continue;
                    }
                    const auto it = channel_producers.find(channel);
                    if (it == channel_producers.end()) {
                        continue;
                    }
                    if (it->second != group_plan.name) {
                        logI << "multi-process cross-group topic, will use shm transport: channel=" << channel << ", producer_group=" << it->second << ", consumer_group=" << group_plan.name;
                    }
                    channel_consumers[channel].insert(group_plan.name);
                }
            }
        }

        const auto log_dir = resolve_log_dir(options);
        const auto ts = make_timestamp();
        auto orchestrator = star::runtime::process::ProcessOrchestrator(g_argv0);
        const auto route_plan = orchestrator.build_route_plan(channel_producers, channel_consumers);

        auto pids = std::vector<pid_t>{};
        auto pid_group = std::unordered_map<pid_t, std::string>{};

        for (auto& [group, paths] : groups) {
            std::sort(paths.begin(), paths.end());
            paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

            auto config = star::runtime::process::WorkerLaunchConfig{};
            config.group = group;
            config.component_conf_paths = paths;
            config.route_plan = route_plan;
            config.frames = options.frames;
            config.sleep_ms = options.sleep_ms;
            if (not log_dir.empty()) {
                config.log_file = log_dir + "/" + group + "_" + ts + ".log";
            } else if (not options.log_file.empty()) {
                config.log_file = options.log_file;
            }

            const auto pid = orchestrator.launch_worker(config);
            if (pid < 0) {
                return 5;
            }
            pids.push_back(pid);
            pid_group.emplace(pid, group);
        }

        const auto result = orchestrator.wait_all(pids, pid_group);
        return result.ok ? 0 : 10;
    }

    auto list(const std::vector<std::string>& module_conf_paths) -> int {
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

    auto graph_modules_dot(const std::vector<std::string>& module_conf_paths) -> int {
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

    auto graph_components_dot(const std::vector<std::string>& module_conf_paths) -> int {
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

    auto doctor(const std::vector<std::string>& module_conf_paths, bool deep) -> int {
        star::runtime::plan::PlanLoader loader;
        bool ok = true;
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
                        auto sym = dlsym(handle, "star_register_components");
                        const auto* err = dlerror();
                        if (err != nullptr || sym == nullptr) {
                            logE << "doctor: dlsym failed: lib=" << lib_key << ", conf=" << component_conf << ", err=" << (err ? err : "unknown");
                            ok = false;
                        }
                        dlclose(handle);
                    }
                }
            }
        }
        return ok ? 0 : 1;
    }

    auto monitor(const RunOptions& options, [[maybe_unused]] const std::vector<std::string>& component_conf_paths) -> int {
        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport_name);
        configure_routed_transport_if_needed(*context, options);
        register_generated_schemas(context->schema_registry());
        star::schema::native::register_business_schemas(context->schema_registry());

        std::cout << "# Starletto Transport Monitor\n";
        std::cout << "# Time: " << make_timestamp() << "\n";
        std::cout << "# Transport: " << options.transport_name << "\n\n";

        const auto stats = context->message_bus().dump_stats_lines();
        if (stats.empty()) {
            std::cout << "(no topics registered)\n";
        } else {
            for (const auto& line : stats) {
                std::cout << line << "\n";
            }
        }

        auto res_mon = star::runtime::observability::ResourceMonitor{};
        const auto self_res = res_mon.sample_self();
        std::cout << "\n" << self_res.format_line() << "\n";

        auto* native_bus = dynamic_cast<star::transport::NativeMessageBus*>(&context->message_bus());
        if (native_bus != nullptr) {
            const auto channels = native_bus->declared_channels();
            std::cout << "\n# Declared channels: " << channels.size() << "\n";
            for (const auto& t : channels) {
                std::cout << "#   " << t << "\n";
            }
        }
        return 0;
    }

  public:
    static auto make_timestamp() -> std::string {
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::string(buf);
    }

  private:
    static void configure_routed_transport_if_needed(star::runtime::RuntimeContext& context, const RunOptions& options) {
        auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context.message_bus());
        if (routed == nullptr) {
            return;
        }
        if (not options.route_plan.empty() || not options.process_group.empty()) {
            context.channel_registry().configure_local_group(options.process_group);
            context.channel_registry().configure_route_plan(options.route_plan);
            routed->configure_route_registry(options.process_group, options.route_plan);
            return;
        }
        routed->configure_route_plan(options.shm_pub_topics, options.shm_sub_topics);
    }

    static void maybe_dump_transport_stats(const star::transport::MessageBus& message_bus) {
        if (const auto* value = std::getenv("STAR_TRANSPORT_STATS"); value == nullptr || std::string(value) != "1") {
            return;
        }
        for (const auto& line : message_bus.dump_stats_lines()) {
            logI << line;
        }
    }

    static auto resolve_log_dir(const RunOptions& options) -> std::string {
        if (not options.log_dir.empty()) {
            return options.log_dir;
        }
        if (not options.log_file.empty()) {
            const auto p = std::filesystem::path(options.log_file);
            if (p.has_parent_path()) {
                return p.parent_path().string();
            }
        }
        return {};
    }

    static auto resolve_library_key(const star::runtime::plan::ComponentLoadPlan& plan) -> std::string {
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
        auto root = star::runtime::process::find_project_root(cp);
        if (not root) {
            return std::nullopt;
        }
        auto component_dir = std::filesystem::weakly_canonical(cp.parent_path(), ec);
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
};

}  // namespace star::native_runner

#endif  // __NATIVE_RUNNER_APP_H__

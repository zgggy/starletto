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


#if defined(STAR_STATIC_COMPONENTS)
extern "C" void star_register_static_components(star::runtime::ComponentRegistry* registry);
#endif

namespace star::native_runner {

    auto App::run(const RunOptions& options, const std::vector<std::string>& component_conf_paths) -> int  {
        if (component_conf_paths.empty()) {
            logE << "no component conf provided";
            return 2;
        }

        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport_name);
        configure_routed_transport_if_needed(*context, options);
        star::schema::native::register_builtin_schemas(context->schema_registry());
        auto* message_bus = &context->message_bus();
        auto registry = std::make_shared<star::runtime::ComponentRegistry>();
        registry->set_schema_registry(&context->schema_registry());
#if defined(STAR_STATIC_COMPONENTS)
        // 静态部署：组件和模块 schema 在加载 plan 前一次性注册；之后 static_link=true
        // 的组件不会再 dlopen。
        star_register_static_components(registry.get());
        logI << "static components registered";
#endif

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
            logI << "loaded component: " << plan->module_name << "." << plan->component_name << ", class=" << plan->component_class;
            if (plan->trigger.has_value() && plan->trigger->cycle_ms.value_or(0) > 0) {
                const auto cycle_ms = plan->trigger->cycle_ms.value_or(0);
                if (min_cycle_ms <= 0 || cycle_ms < min_cycle_ms) {
                    min_cycle_ms = cycle_ms;
                }
            }
        }

        // 服务通道：本进程声明的 commands + supervisor 广播的服务通道列表
        if (auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(message_bus)) {
            auto mark = [routed](const std::string& channel) {
                if (not channel.empty()) {
                    routed->register_system_channel(channel);
                }
            };
            for (const auto& plan : component_plans) {
                if (not plan.services.has_value()) {
                    continue;
                }
                for (const auto& svc : *plan.services) {
                    const auto channel = svc.channel.value_or(svc.name);
                    if (channel.empty()) {
                        continue;
                    }
                    mark(channel + "/request");
                    mark(channel + "/response");
                }
            }
            auto remaining = options.system_channels;
            while (not remaining.empty()) {
                const auto pos = remaining.find(',');
                const auto item = remaining.substr(0, pos);
                mark(item);
                if (pos == std::string::npos) {
                    break;
                }
                remaining = remaining.substr(pos + 1);
            }
        }

        const auto process_plan = star::runtime::plan::build_process_plan(component_plans);
        host.set_process_plan(process_plan);
        host.set_process_group(options.process_group);
        if (not options.record_file.empty()) {
            host.set_record_file(options.record_file);
        }

        auto sleep_ms = options.sleep_ms;
        if (sleep_ms <= 0) {
            sleep_ms = min_cycle_ms;
        }

        host.configure();
        host.start();
        if (options.ready_fd >= 0) {
            const char ready = 'R';
            (void)::write(options.ready_fd, &ready, 1);
            ::close(options.ready_fd);
        }
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

    auto App::run_multi_process(const RunOptions& options, const std::vector<std::string>& component_conf_paths) -> int  {
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

        // 多进程录制必须逐 worker 一个文件：同一路径会被各 worker 的 fopen("wb") 互相截断
        auto recording_requested = false;
        for (const auto& plan : component_plans) {
            if (not plan.outputs.has_value()) {
                continue;
            }
            for (const auto& port : *plan.outputs) {
                if (port.record.value_or(false)) {
                    recording_requested = true;
                }
            }
        }
        if (recording_requested and not options.record_file.empty()) {
            logE << "multi-process recording requires --record-dir=<dir>, not --record-file (one file per worker)";
            return 4;
        }
        if (recording_requested and options.record_dir.empty()) {
            logE << "multi-process recording is enabled by component.conf but --record-dir is missing; "
                 << "refusing to start to avoid workers truncating the same file";
            return 4;
        }
        if (not options.record_dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(options.record_dir, ec);
            if (ec) {
                logE << "cannot create record dir: " << options.record_dir << ", err=" << ec.message();
                return 4;
            }
        }

        const auto log_dir = resolve_log_dir(options);
        const auto ts = make_timestamp();
        auto orchestrator = star::runtime::process::ProcessOrchestrator(g_argv0);
        // 服务通道的「生产者组」无法从 component.conf 推出（客户端组不在 conf 里），
        // 作为系统通道广播给所有 worker：任何进程都可能读或写，始终走 shm
        auto service_channels = std::unordered_set<std::string>{};
        for (const auto& plan : component_plans) {
            if (not plan.services.has_value()) {
                continue;
            }
            for (const auto& svc : *plan.services) {
                const auto channel = svc.channel.value_or(svc.name);
                if (channel.empty()) {
                    continue;
                }
                service_channels.insert(channel + "/request");
                service_channels.insert(channel + "/response");
            }
        }
        auto system_channels_csv = std::string{};
        for (const auto& channel : service_channels) {
            if (not system_channels_csv.empty()) {
                system_channels_csv.push_back(',');
            }
            system_channels_csv += channel;
        }

        const auto route_plan = orchestrator.build_route_plan(channel_producers, channel_consumers);

        auto configs = std::unordered_map<std::string, star::runtime::process::WorkerLaunchConfig>{};
        for (auto& [group, paths] : groups) {
            std::sort(paths.begin(), paths.end());
            paths.erase(std::unique(paths.begin(), paths.end()), paths.end());

            auto config = star::runtime::process::WorkerLaunchConfig{};
            config.group = group;
            config.component_conf_paths = paths;
            config.route_plan = route_plan;
            config.system_channels = system_channels_csv;
            if (not options.record_dir.empty()) {
                config.record_file = options.record_dir + "/" + group + "_" + ts + ".starrec";
            }
            config.frames = options.frames;
            config.sleep_ms = options.sleep_ms;
            if (not log_dir.empty()) {
                config.log_file = log_dir + "/" + group + "_" + ts + ".log";
            } else if (not options.log_file.empty()) {
                config.log_file = options.log_file;
            }
            configs.emplace(group, std::move(config));
        }

        const auto policy = options.restart_on_failure ? star::runtime::process::RestartPolicy::kOnFailure
                                                       : star::runtime::process::RestartPolicy::kNever;
        const auto max_restarts = std::max<int64_t>(0, options.max_restarts);
        const auto backoff_ms = std::max<int64_t>(0, options.restart_backoff_ms);
        const auto ready_timeout_ms = std::max<int64_t>(100, options.ready_timeout_ms);

        auto attempts = std::unordered_map<std::string, int>{};
        auto running = std::unordered_map<pid_t, std::string>{};
        auto result = star::runtime::process::ProcessResult{};

        // 启动一个 group；ready 超时/未就绪也走失败与重启策略
        std::function<void(const std::string&)> start_group = [&](const std::string& group) {
            const auto config_it = configs.find(group);
            if (config_it == configs.end()) {
                result.ok = false;
                result.exited_workers.emplace_back(group, -1);
                return;
            }
            // 重启不覆盖上一次尝试的录制：崩溃现场要保留
            auto launch_config = config_it->second;
            const auto attempt_index = attempts[group];
            if (attempt_index > 0 and not launch_config.record_file.empty()) {
                const auto dot = launch_config.record_file.find_last_of('.');
                const auto suffix = ".attempt" + std::to_string(attempt_index + 1);
                if (dot == std::string::npos) {
                    launch_config.record_file += suffix;
                } else {
                    launch_config.record_file.insert(dot, suffix);
                }
            }
            auto launched = orchestrator.launch_worker(launch_config);
            if (launched.pid <= 0) {
                logE << "[Supervisor] launch failed: group=" << group;
                result.ok = false;
                result.exited_workers.emplace_back(group, -1);
                return;
            }
            if (star::runtime::process::ProcessOrchestrator::wait_ready(launched, static_cast<int>(ready_timeout_ms))) {
                running.emplace(launched.pid, group);
                return;
            }
            logE << "[Supervisor] worker not ready within " << ready_timeout_ms << "ms: group=" << group;
            ::kill(launched.pid, SIGKILL);
            int status = 0;
            (void)::waitpid(launched.pid, &status, 0);
            const auto used = ++attempts[group];
            if (policy == star::runtime::process::RestartPolicy::kOnFailure and used <= max_restarts) {
                const auto delay = star::runtime::process::restart_delay_ms(used, backoff_ms);
                logW << "[Supervisor] restarting group=" << group << " in " << delay << "ms (attempt "
                     << used << "/" << max_restarts << ")";
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                start_group(group);
                return;
            }
            result.ok = false;
            result.exited_workers.emplace_back(group, -1);
        };

        for (const auto& [group, _] : groups) {
            start_group(group);
        }

        while (not running.empty()) {
            int status = 0;
            const auto pid = ::waitpid(-1, &status, 0);
            if (pid < 0) {
                logE << "[Supervisor] waitpid failed";
                result.ok = false;
                break;
            }
            const auto group_it = running.find(pid);
            if (group_it == running.end()) {
                continue;
            }
            const auto group = group_it->second;
            running.erase(group_it);

            const auto exited_ok = WIFEXITED(status) and WEXITSTATUS(status) == 0;
            if (exited_ok) {
                logI << "[Supervisor] worker exited ok: group=" << group;
                result.exited_workers.emplace_back(group, 0);
                continue;
            }

            int code = -1;
            if (WIFEXITED(status)) {
                code = WEXITSTATUS(status);
                logE << "[Supervisor] worker exited non-zero: group=" << group << ", code=" << code;
            } else if (WIFSIGNALED(status)) {
                code = -WTERMSIG(status);
                logE << "[Supervisor] worker killed by signal: group=" << group << ", sig=" << WTERMSIG(status);
            }

            const auto used = ++attempts[group];
            if (policy == star::runtime::process::RestartPolicy::kOnFailure and used <= max_restarts) {
                const auto delay = star::runtime::process::restart_delay_ms(used, backoff_ms);
                logW << "[Supervisor] restarting group=" << group << " in " << delay << "ms (attempt "
                     << used << "/" << max_restarts << ")";
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                start_group(group);
                continue;
            }
            result.ok = false;
            result.exited_workers.emplace_back(group, code);
        }

        if (not result.ok) {
            logE << "[Supervisor] run failed, restart-policy="
                 << (policy == star::runtime::process::RestartPolicy::kOnFailure ? "on-failure" : "never")
                 << ", max-restarts=" << max_restarts;
        }
        return result.ok ? 0 : 10;
    }

}  // namespace star::native_runner

#ifndef __NATIVE_RUNNER_APP_H__
#define __NATIVE_RUNNER_APP_H__

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>

#include "apps/native_runner/runner_utils.h"
#include "apps/native_runner/types.h"
#include "basement/runtime/plan/plan_loader.h"
#include "basement/runtime/plan/plan_builder.h"
#include "basement/runtime/component/native_component_host.h"
#include "basement/runtime/observability/resource_monitor.h"
#include "basement/runtime/process/path_resolver.h"
#include "basement/schema/native/schema_bootstrap.h"
#include "basement/transport/shm/shm_channel.h"
#include "basement/transport/shm/shm_routed_message_bus.h"
#include "basement/transport/shm/shm_service_bus.h"
#include "basement/transport/shm/shm_views.h"
#include "libraries/common/schema/schema_value.h"
#include "modules/control/pid/control_component.h"
#include "modules/planning/motion/motion_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

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
        star::schema::native::register_builtin_schemas(context->schema_registry());
        star::setting::schema::register_setting_schemas(context->schema_registry());
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

    auto selftest_transport(RunOptions options) -> int {
        if (options.transport_name.empty()) {
            options.transport_name = "shm_routed";
        }
        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport_name);

        const auto channel = "selftest.transport.routed." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto mode_channel = "selftest.transport.mode." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto degrade_channel = "selftest.transport.degrade." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        options.shm_pub_topics = channel + "," + mode_channel + "," + degrade_channel;
        options.shm_sub_topics = channel + "," + mode_channel + "," + degrade_channel;
        options.route_plan.clear();
        options.process_group.clear();
        configure_routed_transport_if_needed(*context, options);

        auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context->message_bus());
        if (routed == nullptr) {
            logE << "selftest_transport requires shm_routed transport, got: " << options.transport_name;
            return 2;
        }

        auto descriptor = star::transport::ChannelDescriptor{};
        descriptor.name = channel;
        descriptor.type_name = "selftest";
        descriptor.schema_type_id = star::schema::SchemaTypeId{};
        descriptor.options = star::transport::ChannelOptions{};

        auto received = std::atomic<int64_t>{0};
        auto reader = routed->create_reader(descriptor, [&received](const star::transport::MessageView&) { received.fetch_add(1, std::memory_order_relaxed); });
        if (!reader) {
            logE << "selftest_transport failed to create reader";
            return 3;
        }
        auto writer = routed->create_writer(descriptor);
        if (!writer) {
            logE << "selftest_transport failed to create writer";
            return 3;
        }

        const auto payload = std::vector<uint8_t>(static_cast<size_t>(16), static_cast<uint8_t>(0x5A));
        auto serializer = star::transport::MessageSerializer{};
        serializer.payload_size_hint = payload.size();
        serializer.encode_binary = [payload]() -> std::vector<uint8_t> { return payload; };

        auto view = star::transport::MessageView{};
        view.payload = star::base::TypedConstPtr{};
        view.schema_type_id = star::schema::SchemaTypeId{};
        view.correlation_id = star::transport::CorrelationId{};

        auto msg = star::transport::SerializedMessage{};
        msg.view = view;
        msg.serializer = std::move(serializer);

        if (routed->shm_available()) {
            auto mode_desc = star::transport::ChannelDescriptor{};
            mode_desc.name = mode_channel;
            mode_desc.type_name = "selftest";
            mode_desc.schema_type_id = star::schema::SchemaTypeId{};
            mode_desc.options = star::transport::ChannelOptions{};

            auto mode_writer = routed->create_writer(mode_desc);
            if (!mode_writer) {
                logE << "selftest_transport failed to create mode writer";
                return 3;
            }

            auto view_only = star::transport::MessageView{};
            view_only.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(1));
            view_only.schema_type_id = star::schema::SchemaTypeId{};
            view_only.correlation_id = star::transport::CorrelationId{};

            const auto legacy_r = mode_writer->write(view_only);
            if (legacy_r.status != star::transport::WriteStatus::SERIALIZER_REQUIRED || !legacy_r.shm_required || !legacy_r.inproc_ok) {
                logE << "selftest_transport legacy(shm_read_write) write(view) unexpected: status=" << star::transport::write_status_string(legacy_r.status)
                     << ", shm_required=" << legacy_r.shm_required << ", inproc_ok=" << legacy_r.inproc_ok;
                return 4;
            }
            logI << "selftest_transport ok: legacy(shm_read_write) write(view)->SERIALIZER_REQUIRED";

            auto degrade_desc = star::transport::ChannelDescriptor{};
            degrade_desc.name = degrade_channel;
            degrade_desc.type_name = "selftest";
            degrade_desc.schema_type_id = star::schema::SchemaTypeId{};
            degrade_desc.options = star::transport::ChannelOptions{};

            auto degrade_writer = routed->create_writer(degrade_desc);
            if (!degrade_writer) {
                logE << "selftest_transport failed to create degrade writer";
                return 3;
            }

            auto bad = star::transport::SerializedMessage{};
            bad.view.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(2));
            bad.view.schema_type_id = star::schema::SchemaTypeId{};
            bad.view.correlation_id = star::transport::CorrelationId{};
            bad.serializer.payload_size_hint = 0;
            bad.serializer.encode_binary = []() -> std::vector<uint8_t> { return {}; };

            const auto bad_r = degrade_writer->write_serialized(bad);
            if (bad_r.status != star::transport::WriteStatus::ENCODE_FAILED || !bad_r.shm_required || !bad_r.inproc_ok) {
                logE << "selftest_transport degrade write_serialized(bad) unexpected: status=" << star::transport::write_status_string(bad_r.status)
                     << ", shm_required=" << bad_r.shm_required << ", inproc_ok=" << bad_r.inproc_ok;
                return 4;
            }

            auto good = msg;
            good.view.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(3));
            const auto degraded_r = degrade_writer->write_serialized(good);
            if (degraded_r.status != star::transport::WriteStatus::SHM_DEGRADED || !degraded_r.shm_required || !degraded_r.inproc_ok) {
                logE << "selftest_transport degrade write_serialized(good) unexpected: status=" << star::transport::write_status_string(degraded_r.status)
                     << ", shm_required=" << degraded_r.shm_required << ", inproc_ok=" << degraded_r.inproc_ok
                     << ", err=" << degraded_r.error_message;
                return 4;
            }
            logI << "selftest_transport ok: degrade fallback ENCODE_FAILED->SHM_DEGRADED";
        } else {
            logW << "selftest_transport: shm not available, skipping legacy(shm_read_write)/degrade checks";
        }

        const auto kWrites = int64_t{20};
        for (int64_t i = 0; i < kWrites; ++i) {
            const auto r = writer->write_serialized(msg);
            if (!r.ok()) {
                logE << "selftest_transport write_serialized failed: status=" << star::transport::write_status_string(r.status) << ", err=" << r.error_message;
                return 4;
            }
        }

        const auto got = received.load(std::memory_order_relaxed);
        if (got != kWrites) {
            logE << "selftest_transport failed: duplicate delivery detected: expected=" << kWrites << ", got=" << got;
            return 1;
        }
        logI << "selftest_transport ok: routed inproc delivery count=" << got;

        if (routed->shm_available()) {
            const auto route_channel = "selftest.transport.route_plan." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            routed->configure_route_registry("planner", route_channel + "|planner|control");

            auto route_desc = star::transport::ChannelDescriptor{};
            route_desc.name = route_channel;
            route_desc.type_name = "selftest";
            route_desc.schema_type_id = star::schema::SchemaTypeId{};
            route_desc.options = star::transport::ChannelOptions{};

            auto route_writer = routed->create_writer(route_desc);
            if (!route_writer) {
                logE << "selftest_transport failed to create route_plan writer";
                return 3;
            }

            auto view_only = star::transport::MessageView{};
            view_only.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(11));
            view_only.schema_type_id = star::schema::SchemaTypeId{};
            view_only.correlation_id = star::transport::CorrelationId{};

            const auto view_r = route_writer->write(view_only);
            if (view_r.status != star::transport::WriteStatus::SERIALIZER_REQUIRED || !view_r.shm_required || !view_r.inproc_ok) {
                logE << "selftest_transport route_plan write(view) unexpected: status=" << star::transport::write_status_string(view_r.status)
                     << ", shm_required=" << view_r.shm_required << ", inproc_ok=" << view_r.inproc_ok;
                return 4;
            }

            auto route_msg = msg;
            route_msg.view.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(22));
            const auto shm_r = route_writer->write_serialized(route_msg);
            if (!shm_r.ok() || !shm_r.shm_ok || !shm_r.shm_required || !shm_r.inproc_ok) {
                logE << "selftest_transport route_plan write_serialized unexpected: status=" << star::transport::write_status_string(shm_r.status)
                     << ", shm_ok=" << shm_r.shm_ok << ", shm_required=" << shm_r.shm_required << ", inproc_ok=" << shm_r.inproc_ok
                     << ", err=" << shm_r.error_message;
                return 4;
            }
            logI << "selftest_transport ok: route_plan write(view)->SERIALIZER_REQUIRED, write_serialized->OK";
        } else {
            logW << "selftest_transport: shm not available, skipping route_plan write semantics check";
        }

        if (routed->shm_available()) {
            const auto service_name = "selftest.service." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            const auto req_topic = service_name + "/request";
            const auto resp_topic = service_name + "/response";

            auto server_bus = std::make_shared<star::transport::ShmRoutedMessageBus>();
            auto client_bus = std::make_shared<star::transport::ShmRoutedMessageBus>();
            if (server_bus->shm_available() && client_bus->shm_available()) {
                const auto route_plan = req_topic + "|planner|control;" + resp_topic + "|control|planner";
                server_bus->configure_route_registry("control", route_plan);
                client_bus->configure_route_registry("planner", route_plan);

                auto server = star::transport::ShmServiceBus(server_bus).create_server(
                    star::transport::ServiceDescriptor{service_name},
                    [](const star::transport::ServiceRequest& req) -> star::transport::SerializedServiceResponse {
                        auto bytes = std::vector<uint8_t>{};
                        if (const auto slot = req.body.payload.as<star::transport::ShmSlotView>()) {
                            bytes.assign(slot->data, slot->data + slot->size);
                        } else if (const auto bin = req.body.payload.as<star::transport::BinaryPayloadView>()) {
                            bytes.assign(bin->data, bin->data + bin->size);
                        }
                        auto resp = star::transport::ServiceResponse{};
                        resp.ok = true;
                        auto payload = std::make_shared<const std::vector<uint8_t>>(bytes);
                        resp.body.payload = star::base::TypedConstPtr::from(payload);
                        auto serializer = star::transport::MessageSerializer{};
                        serializer.payload_size_hint = payload->size();
                        serializer.encode_binary = [payload]() -> std::vector<uint8_t> { return *payload; };
                        return star::transport::SerializedServiceResponse{resp, std::move(serializer)};
                    });

                auto client = star::transport::ShmServiceBus(client_bus).create_client(star::transport::ServiceDescriptor{service_name});
                if (!client || !client->available()) {
                    logE << "selftest_transport failed: service client not available";
                    return 5;
                }

                auto body = star::transport::ServiceRequest{};
                body.body.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(123));
                body.body.correlation_id = star::transport::CorrelationId{};

                const auto kCalls = int64_t{200};
                for (int64_t i = 0; i < kCalls; ++i) {
                    auto payload = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>(static_cast<size_t>(16), static_cast<uint8_t>(i & 0xFF)));
                    auto req = star::transport::ServiceRequest{};
                    req.body.payload = star::base::TypedConstPtr::from(payload);
                    req.body.correlation_id = star::transport::CorrelationId::generate();

                    auto serializer = star::transport::MessageSerializer{};
                    serializer.payload_size_hint = payload->size();
                    serializer.encode_binary = [payload]() -> std::vector<uint8_t> { return *payload; };

                    const auto r = client->call_serialized(star::transport::SerializedServiceRequest{req, std::move(serializer)});
                    if (!r.ok) {
                        logE << "selftest_transport failed: service call failed: " << r.message;
                        return 5;
                    }
                    if (const auto slot = r.body.payload.as<star::transport::ShmSlotView>()) {
                        if (slot->size != payload->size() || std::memcmp(slot->data, payload->data(), payload->size()) != 0) {
                            logE << "selftest_transport failed: service response mismatch";
                            return 5;
                        }
                    } else if (const auto bin = r.body.payload.as<star::transport::BinaryPayloadView>()) {
                        if (bin->size != payload->size() || std::memcmp(bin->data, payload->data(), payload->size()) != 0) {
                            logE << "selftest_transport failed: service response mismatch";
                            return 5;
                        }
                    } else {
                        logE << "selftest_transport failed: service response payload type unexpected";
                        return 5;
                    }
                }

                logI << "selftest_transport ok: service soak calls=" << kCalls;
            } else {
                logW << "selftest_transport: shm not available for service soak, skipping";
            }
        }

        const auto shm_ok = selftest_shm_channel_multiprocess();
        if (shm_ok != 0) {
            return shm_ok;
        }
        return 0;
    }

    auto monitor(const RunOptions& options, [[maybe_unused]] const std::vector<std::string>& component_conf_paths) -> int {
        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport_name);
        configure_routed_transport_if_needed(*context, options);
        star::schema::native::register_builtin_schemas(context->schema_registry());
        star::setting::schema::register_setting_schemas(context->schema_registry());

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
    static auto selftest_shm_channel_multiprocess() -> int {
        struct Payload {
            uint64_t writer_id{0};
            uint64_t seq{0};
            uint64_t magic{0x5A5A5A5A5A5A5A5AULL};
        };

        static constexpr uint64_t kMagic = 0x5A5A5A5A5A5A5A5AULL;

        const auto name = std::string("/starletto_selftest_shm_") + std::to_string(::getpid()) + "_" +
            std::to_string(static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count()));

        constexpr uint32_t kCapacity = 1024 * 1024;

        constexpr uint64_t kWritesPerWriter = 8000;
        constexpr int64_t kTimeoutMs = 3000;

        const auto spawn_writer = [&](uint64_t writer_id) -> pid_t {
            const auto pid = ::fork();
            if (pid < 0) {
                return -1;
            }
            if (pid == 0) {
                auto ch = std::make_shared<star::transport::ShmChannel>(name, kCapacity);
                for (uint64_t i = 0; i < kWritesPerWriter; ++i) {
                    auto loan = ch->loan(sizeof(Payload));
                    const auto seq = loan.seq;
                    auto payload = Payload{};
                    payload.writer_id = writer_id;
                    payload.seq = seq;
                    payload.magic = kMagic;
                    std::memcpy(loan.data(), &payload, sizeof(payload));
                    loan.commit(sizeof(payload));
                }
                _exit(0);
            }
            return pid;
        };

        const auto w1 = spawn_writer(1);
        const auto w2 = spawn_writer(2);
        if (w1 < 0 || w2 < 0) {
            if (w1 > 0) { ::kill(w1, SIGKILL); }
            if (w2 > 0) { ::kill(w2, SIGKILL); }
            if (w1 > 0) { ::waitpid(w1, nullptr, 0); }
            if (w2 > 0) { ::waitpid(w2, nullptr, 0); }
            logE << "selftest_shm failed: fork failed";
            return 2;
        }

        auto channel = std::make_shared<star::transport::ShmChannel>(name, kCapacity);

        const auto start = std::chrono::steady_clock::now();
        uint64_t last_seq = 0;
        uint64_t checked = 0;
        uint64_t bad = 0;
        uint64_t overwritten = 0;
        const auto slot_count = channel->slot_count();
        bool w1_done = false;
        bool w2_done = false;

        while (true) {
            for (uint64_t i = 0; i < 16; ++i) {
                auto r = channel->read(&last_seq);
                if (!r.has_value()) {
                    break;
                }
                const auto* ptr = channel->payload_ptr(r->slot);
                if (ptr == nullptr || r->size < sizeof(Payload)) {
                    bad += 1;
                    continue;
                }
                auto payload = Payload{};
                std::memcpy(&payload, ptr, sizeof(payload));
                if (payload.magic != kMagic) {
                    bad += 1;
                    continue;
                }
                if (payload.seq != r->seq) {
                    const auto cur = channel->current_seq();
                    if (slot_count > 0 && cur >= r->seq + slot_count) {
                        overwritten += 1;
                        continue;
                    }
                    bad += 1;
                    continue;
                }
                if (payload.writer_id != 1 && payload.writer_id != 2) {
                    bad += 1;
                    continue;
                }
                checked += 1;
            }

            int status = 0;
            if (!w1_done) {
                const auto w = ::waitpid(w1, &status, WNOHANG);
                if (w == w1) {
                    w1_done = true;
                }
            }
            if (!w2_done) {
                const auto w = ::waitpid(w2, &status, WNOHANG);
                if (w == w2) {
                    w2_done = true;
                }
            }
            const auto elapsed_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
            if (w1_done && w2_done) {
                break;
            }
            if (elapsed_ms > kTimeoutMs) {
                ::kill(w1, SIGKILL);
                ::kill(w2, SIGKILL);
                ::waitpid(w1, nullptr, 0);
                ::waitpid(w2, nullptr, 0);
                logE << "selftest_shm failed: timeout waiting for updates";
                return 3;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        if (bad > 0) {
            logE << "selftest_shm failed: detected corrupted reads: bad=" << bad << ", checked=" << checked;
            return 1;
        }
        logI << "selftest_shm ok: checked=" << checked << ", overwritten=" << overwritten;
        return 0;
    }

    static void configure_routed_transport_if_needed(star::runtime::RuntimeContext& context, const RunOptions& options) {
        auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context.message_bus());
        if (routed == nullptr) {
            return;
        }
        if (not options.route_plan.empty() || not options.process_group.empty()) {
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

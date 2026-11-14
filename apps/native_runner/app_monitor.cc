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


namespace star::native_runner {

    auto App::monitor(const RunOptions& options, [[maybe_unused]] const std::vector<std::string>& component_conf_paths) -> int  {
        using WorkerStats = star::generated::observability::worker_stats::WorkerStats;
        const auto transport_name = options.transport_name.empty() ? std::string("shm_routed") : options.transport_name;
        auto context = star::runtime::RuntimeFactory::create_native_context(transport_name);

        // monitor 是普通订阅者：只订阅统计通道，不再创建「自己的空总线」自指
        if (auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context->message_bus())) {
            if (options.route_plan.empty() && options.process_group.empty()) {
                const auto sub = (options.shm_sub_topics.empty() ? std::string{} : options.shm_sub_topics + ",") +
                                 star::runtime::observability::kStatsChannelName + "," + star::runtime::observability::kHealthChannelName;
                routed->configure_route_plan(options.shm_pub_topics, sub);
            } else {
                routed->configure_route_registry(options.process_group, options.route_plan);
            }
        }
        star::schema::native::register_builtin_schemas(context->schema_registry());

        using WorkerHealth = star::generated::observability::worker_health::WorkerHealth;
        struct LatestStats {
            std::mutex mutex;
            std::map<std::string, WorkerStats> by_group;
            std::map<std::string, WorkerHealth> health_by_component;
            std::atomic<uint64_t> received{0};
            std::atomic<uint64_t> health_received{0};
            std::atomic<uint64_t> decode_failures{0};
        };
        auto latest = std::make_shared<LatestStats>();

        auto descriptor = star::transport::ChannelDescriptor{};
        descriptor.name = star::runtime::observability::kStatsChannelName;
        descriptor.type_name = "observability.worker_stats";
        descriptor.schema_type_id = WorkerStats::schema_type_id();
        auto reader = context->message_bus().create_reader(descriptor, [latest](const star::transport::MessageView& message) {
            std::vector<uint8_t> bytes;
            if (const auto* binary = message.payload.as<star::transport::BinaryPayloadView>().get()) {
                bytes.assign(binary->data, binary->data + binary->size);
            } else if (const auto slot = message.payload.as<star::transport::ShmSlotView>()) {
                if (not slot->stable()) {
                    return;
                }
                const auto view = slot->bytes();
                bytes.assign(view.begin(), view.end());
            } else {
                return;
            }
            try {
                auto stats = WorkerStats::from_binary(std::span<const uint8_t>(bytes.data(), bytes.size()));
                {
                    std::lock_guard<std::mutex> lock(latest->mutex);
                    latest->by_group[stats.process_group] = std::move(stats);
                }
                latest->received.fetch_add(1, std::memory_order_relaxed);
            } catch (const std::exception& e) {
                latest->decode_failures.fetch_add(1, std::memory_order_relaxed);
                logW << "monitor: decode worker stats failed: " << e.what();
            }
        });

        auto health_descriptor = star::transport::ChannelDescriptor{};
        health_descriptor.name = star::runtime::observability::kHealthChannelName;
        health_descriptor.type_name = "observability.worker_health";
        health_descriptor.schema_type_id = WorkerHealth::schema_type_id();
        auto health_reader = context->message_bus().create_reader(health_descriptor, [latest](const star::transport::MessageView& message) {
            try {
                auto health = WorkerHealth{};
                if (const auto typed = message.payload.as<WorkerHealth>()) {
                    health = *typed;
                } else if (const auto binary = message.payload.as<star::transport::BinaryPayloadView>()) {
                    health = WorkerHealth::from_binary(binary->bytes());
                } else if (const auto slot = message.payload.as<star::transport::ShmSlotView>()) {
                    if (not slot->stable()) {
                        return;
                    }
                    health = WorkerHealth::from_binary(slot->bytes());
                } else {
                    return;
                }
                {
                    std::lock_guard<std::mutex> lock(latest->mutex);
                    latest->health_by_component[health.component] = std::move(health);
                }
                latest->health_received.fetch_add(1, std::memory_order_relaxed);
            } catch (const std::exception& e) {
                latest->decode_failures.fetch_add(1, std::memory_order_relaxed);
                logW << "monitor: decode worker health failed: " << e.what();
            }
        });

        std::cout << "# Starletto Transport Monitor\n";
        std::cout << "# Time: " << make_timestamp() << "\n";
        std::cout << "# Transport: " << transport_name << "\n";
        std::cout << "# Stats channel: " << star::runtime::observability::kStatsChannelName << "\n";
        std::cout << std::endl;

        const auto duration_ms = options.monitor_ms > 0 ? options.monitor_ms : 3000;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration_ms);
        auto last_received = uint64_t{0};
        auto printed_any = false;
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            auto snapshot = std::map<std::string, WorkerStats>{};
            auto health_snapshot = std::map<std::string, WorkerHealth>{};
            {
                std::lock_guard<std::mutex> lock(latest->mutex);
                snapshot = latest->by_group;
                health_snapshot = latest->health_by_component;
            }
            const auto received_now = latest->received.load(std::memory_order_relaxed) +
                                      latest->health_received.load(std::memory_order_relaxed);
            const auto now = std::chrono::steady_clock::now();
            if ((not snapshot.empty() or not health_snapshot.empty()) and (options.monitor_once or received_now != last_received)) {
                std::cout << "group      pid      transport   channel                                        hz      p99_us   dropped  failures\n";
                for (const auto& [group, stats] : snapshot) {
                    for (const auto& entry : stats.channels) {
                        std::cout << std::left << std::setw(10) << group << " " << std::setw(8) << stats.pid << " "
                                  << std::setw(11) << stats.transport.value_or("") << " " << std::setw(45)
                                  << entry.channel << " " << std::setw(7) << entry.avg_hz << " "
                                  << std::setw(8) << entry.latency_p99_us << " " << std::setw(8)
                                  << entry.dropped_count << " " << std::setw(8)
                                  << entry.callback_failures << "\n";
                    }
                }
                auto unhealthy = std::vector<const WorkerHealth*>{};
                for (const auto& [_, health] : health_snapshot) {
                    if (not health.healthy) {
                        unhealthy.push_back(&health);
                    }
                }
                if (not unhealthy.empty()) {
                    std::cout << "health     component                 group        severity  message\n";
                    for (const auto* health : unhealthy) {
                        std::cout << "health     " << std::left << std::setw(24) << health->component << " "
                                  << std::setw(12) << health->process_group.value_or("") << " "
                                  << std::setw(9) << health->severity << " " << health->message << "\n";
                    }
                }
                std::cout << std::endl;
                last_received = received_now;
                printed_any = true;
            }
            if (options.monitor_once) {
                if (printed_any or now >= deadline) {
                    break;
                }
                continue;
            }
            if (now >= deadline) {
                break;
            }
        }
        if (not printed_any) {
            std::cout << "(no worker stats received on " << star::runtime::observability::kStatsChannelName
                      << "; is a worker running with a matching transport?)\n";
        }

        auto res_mon = star::runtime::observability::ResourceMonitor{};
        const auto self_res = res_mon.sample_self();
        std::cout << "\n" << self_res.format_line() << "\n";
        std::cout << "# worker stats received: " << latest->received.load() << ", decode failures: " << latest->decode_failures.load() << "\n";
        return 0;
    }

}  // namespace star::native_runner

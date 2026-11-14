#ifndef __STAR_RUNTIME_OBSERVABILITY_HEALTH_MONITOR_H__
#define __STAR_RUNTIME_OBSERVABILITY_HEALTH_MONITOR_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <unistd.h>

#include "basement/core/logger/logger.h"
#include "basement/runtime/observability/health_checker.h"
#include "basement/runtime/observability/resource_monitor.h"
#include "basement/schema/builtin/observability/worker_health.s.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace star::runtime::observability {

inline constexpr const char* kHealthChannelName = "starletto.health";

// 把 HealthChecker 从「只有单测」变成运行时能力：
// 周期评估每个组件（频率 + 资源），严重度变化时写日志，并把报告发布到普通通道
// starletto.health，供 monitor / 录制 / 外部工具消费。
class HealthMonitor final {
  public:
    struct Rule {
        std::string component;
        ChannelHealthRule channel{};
        ResourceHealthRule resource{};
        bool has_channel{false};
    };

    HealthMonitor(star::transport::MessageBus& bus,
                  std::string process_group,
                  std::vector<Rule> rules,
                  int64_t interval_ms = 2000)
        : bus_(&bus),
          process_group_(std::move(process_group)),
          rules_(std::move(rules)),
          interval_ms_(interval_ms > 0 ? interval_ms : 2000),
          checker_(bus),
          resource_monitor_(std::make_shared<ResourceMonitor>()) {
        checker_.set_resource_monitor(resource_monitor_);
        for (const auto& rule : rules_) {
            auto component_rule = ComponentHealthRule{};
            component_rule.name = rule.component;
            if (rule.has_channel) {
                component_rule.channel_rule = rule.channel;
            }
            component_rule.resource_rule = rule.resource;
            checker_.add_component_rule(component_rule);
        }
    }

    ~HealthMonitor() { stop(); }

    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;

    void start() {
        if (rules_.empty() or thread_.joinable()) {
            return;
        }
        if (auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(bus_)) {
            routed->register_system_publisher(kHealthChannelName);
        }
        running_.store(true, std::memory_order_relaxed);
        thread_ = std::thread([this]() { loop(); });
    }

    void stop() {
        running_.store(false, std::memory_order_relaxed);
        wait_cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
        writer_.reset();
    }

    // 测试与手工诊断入口：立即评估一轮并发布
    auto evaluate_once() -> std::vector<ComponentHealthReport> {
        auto reports = std::vector<ComponentHealthReport>{};
        reports.reserve(rules_.size());
        for (const auto& rule : rules_) {
            auto report = checker_.check_component(rule.component, static_cast<int64_t>(::getpid()));
            report_component_change(report);
            publish(report);
            reports.push_back(std::move(report));
        }
        return reports;
    }

  private:
    static auto severity_name(HealthSeverity severity) -> const char* {
        switch (severity) {
            case HealthSeverity::OK:
                return "ok";
            case HealthSeverity::WARNING:
                return "warn";
            case HealthSeverity::CRITICAL:
                return "crit";
        }
        return "unknown";
    }

    void loop() {
        while (running_.load(std::memory_order_relaxed)) {
            evaluate_once();
            std::unique_lock<std::mutex> lock(wait_mutex_);
            wait_cv_.wait_for(lock, std::chrono::milliseconds(interval_ms_),
                              [this]() { return not running_.load(std::memory_order_relaxed); });
        }
    }

    void report_component_change(const ComponentHealthReport& report) {
        const auto it = last_severity_.find(report.component_name);
        const auto previous = it == last_severity_.end() ? HealthSeverity::OK : it->second;
        const auto changed = it == last_severity_.end() or it->second != report.max_severity;
        last_severity_[report.component_name] = report.max_severity;
        if (not changed) {
            return;
        }
        // 首次评估健康不需要刷日志；从异常恢复才值得记一条
        if (report.max_severity == HealthSeverity::OK) {
            if (previous != HealthSeverity::OK) {
                logI << "health recovered: " << report.format_line();
            }
            return;
        }
        if (report.max_severity == HealthSeverity::CRITICAL) {
            logE << "health critical: " << report.format_line();
        } else {
            logW << "health warning: " << report.format_line();
        }
    }

    void publish(const ComponentHealthReport& report) {
        auto writer = writer_for_health_channel();
        if (not writer) {
            return;
        }
        auto message = star::generated::observability::worker_health::WorkerHealth{};
        message.component = report.component_name;
        message.healthy = report.overall_healthy;
        message.severity = severity_name(report.max_severity);
        message.message = report.summary_message;
        message.pid = report.resource_status.pid > 0 ? report.resource_status.pid : static_cast<int64_t>(::getpid());
        message.timestamp_ns = report.check_timestamp_ns;
        message.cpu_percent = report.resource_status.cpu_percent;
        message.rss_kb = report.resource_status.rss_kb;
        message.fd_count = report.resource_status.fd_count;
        message.process_group = process_group_;
        if (not report.channel_statuses.empty()) {
            message.channel = report.channel_statuses.front().channel;
            message.actual_hz = report.channel_statuses.front().actual_hz;
        }

        auto msg = std::make_shared<const star::generated::observability::worker_health::WorkerHealth>(std::move(message));
        auto serialized = star::transport::SerializedMessage{};
        serialized.view.payload = star::base::TypedConstPtr::from(msg);
        serialized.view.schema_type_id = msg->schema_type_id();
        serialized.serializer.payload_size_hint = 0;
        serialized.serializer.encode_binary = [msg]() { return msg->encode_binary(); };
        const auto result = writer->write_serialized(serialized);
        if (not result.ok()) {
            logW << "HealthMonitor publish failed: status=" << star::transport::write_status_string(result.status)
                 << ", err=" << result.error_message;
        }
    }

    auto writer_for_health_channel() -> std::shared_ptr<star::transport::Writer> {
        if (writer_) {
            return writer_;
        }
        auto descriptor = star::transport::ChannelDescriptor{};
        descriptor.name = kHealthChannelName;
        descriptor.type_name = "observability.worker_health";
        descriptor.schema_type_id = star::generated::observability::worker_health::WorkerHealth::schema_type_id();
        writer_ = bus_->create_writer(descriptor);
        return writer_;
    }

    star::transport::MessageBus* bus_;
    std::string process_group_;
    std::vector<Rule> rules_;
    int64_t interval_ms_;
    HealthChecker checker_;
    std::shared_ptr<ResourceMonitor> resource_monitor_;
    std::shared_ptr<star::transport::Writer> writer_;

    std::unordered_map<std::string, HealthSeverity> last_severity_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
};

}  // namespace star::runtime::observability

#endif  // __STAR_RUNTIME_OBSERVABILITY_HEALTH_MONITOR_H__

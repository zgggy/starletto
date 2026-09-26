#ifndef __BASEMENT_HEALTH_CHECKER_H__
#define __BASEMENT_HEALTH_CHECKER_H__

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/runtime/observability/resource_monitor.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/common/channel_stats.h"

namespace star::runtime::observability {

enum class HealthSeverity : uint8_t {
    OK = 0,
    WARNING = 1,
    CRITICAL = 2,
};

struct ChannelHealthRule {
    std::string channel;
    double min_hz{0};
    uint64_t window_ms{3000};
};

struct ResourceHealthRule {
    double max_cpu_percent{90.0};
    uint64_t max_rss_mb{512};
    int max_fd_count{256};
};

struct ComponentHealthRule {
    std::string name;
    ChannelHealthRule channel_rule;
    ResourceHealthRule resource_rule;
};

struct ChannelHealthStatus {
    std::string channel;
    bool healthy{true};
    double actual_hz{0};
    std::string message;
    HealthSeverity severity{HealthSeverity::OK};
};

struct ResourceHealthStatus {
    int64_t pid{0};
    std::string name;
    bool healthy{true};
    double cpu_percent{0};
    uint64_t rss_kb{0};
    int fd_count{0};
    std::string message;
    HealthSeverity severity{HealthSeverity::OK};
};

struct ComponentHealthReport {
    std::string component_name;
    bool overall_healthy{true};
    std::vector<ChannelHealthStatus> channel_statuses;
    ResourceHealthStatus resource_status;
    std::string summary_message;
    HealthSeverity max_severity{HealthSeverity::OK};
    int64_t check_timestamp_ns{0};

    auto format_line() const -> std::string {
        const char* sev_str = "OK";
        if (max_severity == HealthSeverity::WARNING) {
            sev_str = "WARN";
        }
        if (max_severity == HealthSeverity::CRITICAL) {
            sev_str = "CRIT";
        }
        char buf[512]{};
        std::snprintf(buf, sizeof(buf), "health component=%s status=%s severity=%s channels=%zu cpu=%.1f%% rss=%lukB fds=%d msg=%s", component_name.c_str(), overall_healthy ? "ok" : "unhealthy",
                      sev_str, channel_statuses.size(), resource_status.cpu_percent, static_cast<unsigned long>(resource_status.rss_kb), resource_status.fd_count, summary_message.c_str());
        return std::string(buf);
    }
};

using UnhealthyAction = std::function<void(const ComponentHealthReport&)>;

class HealthChecker {
  public:
    explicit HealthChecker(std::shared_ptr<star::transport::MessageBus> bus) : bus_(std::move(bus)) {}

    void add_channel_rule(const ChannelHealthRule& rule) { channel_rules_.push_back(rule); }

    void add_component_rule(const ComponentHealthRule& rule) { component_rules_[rule.name] = rule; }

    void set_resource_monitor(std::shared_ptr<ResourceMonitor> monitor) { res_monitor_ = std::move(monitor); }

    void set_unhealthy_action(UnhealthyAction action) { unhealthy_action_ = std::move(action); }

    auto check_all_channels() const -> std::vector<ChannelHealthStatus> {
        auto results = std::vector<ChannelHealthStatus>{};
        const auto all_stats = bus_->all_channel_stats();
        auto stats_map = build_stats_map(all_stats);
        for (const auto& rule : channel_rules_) {
            results.push_back(check_channel_rule(rule, stats_map));
        }
        return results;
    }

    auto check_resource(int64_t pid, const ResourceHealthRule& rule = {}) const -> ResourceHealthStatus {
        if (res_monitor_) {
            return check_resource_with_monitor(pid, rule);
        }
        return check_resource_fallback(pid, rule);
    }

    auto check_component(const std::string& name, int64_t pid) const -> ComponentHealthReport {
        auto report = ComponentHealthReport{};
        report.component_name = name;
        report.check_timestamp_ns = steady_now_ns();

        const auto comp_it = component_rules_.find(name);
        const ResourceHealthRule res_rule = (comp_it != component_rules_.end()) ? comp_it->second.resource_rule : ResourceHealthRule{};

        report.resource_status = check_resource(pid, res_rule);
        if (!report.resource_status.healthy) {
            report.overall_healthy = false;
            report.max_severity = std::max(report.max_severity, report.resource_status.severity);
        }

        const auto all_stats = bus_->all_channel_stats();
        auto stats_map = build_stats_map(all_stats);
        // 评估组件自己的 channel_rule：ComponentHealthRule 承诺按组件配置通道健康，
        // 此前却遍历全局 channel_rules_，导致 per-component 规则被静默忽略
        if (comp_it != component_rules_.end()) {
            auto ts = check_channel_rule(comp_it->second.channel_rule, stats_map);
            report.channel_statuses.push_back(std::move(ts));
            if (!report.channel_statuses.back().healthy) {
                report.overall_healthy = false;
                report.max_severity = std::max(report.max_severity, report.channel_statuses.back().severity);
            }
        }

        build_summary(&report);
        if (!report.overall_healthy && unhealthy_action_) {
            unhealthy_action_(report);
        }

        return report;
    }

    auto check_all_components() const -> std::vector<ComponentHealthReport> {
        auto reports = std::vector<ComponentHealthReport>{};
        for (const auto& [name, rule] : component_rules_) {
            const auto snap = res_monitor_ ? res_monitor_->sample(getpid()) : ResourceSnapshot{};
            reports.push_back(check_component(name, snap.pid > 0 ? snap.pid : getpid()));
        }
        return reports;
    }

    auto dump_health_lines() const -> std::vector<std::string> {
        auto lines = std::vector<std::string>{};
        lines.emplace_back("[Health] component | status | severity | channels | cpu% | rss_kB | fds | message");
        const auto reports = check_all_components();
        for (const auto& r : reports) {
            lines.push_back(r.format_line());
        }
        return lines;
    }

  private:
    static auto steady_now_ns() -> int64_t { return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

    static auto build_stats_map(const std::vector<star::transport::ChannelStatsSnapshot>& all_stats) -> std::unordered_map<std::string, star::transport::ChannelStatsSnapshot> {
        auto out = std::unordered_map<std::string, star::transport::ChannelStatsSnapshot>{};
        out.reserve(all_stats.size());
        for (const auto& snap : all_stats) {
            out[snap.channel_name] = snap;
        }
        return out;
    }

    static auto check_channel_rule(const ChannelHealthRule& rule, const std::unordered_map<std::string, star::transport::ChannelStatsSnapshot>& stats) -> ChannelHealthStatus {
        auto status = ChannelHealthStatus{};
        status.channel = rule.channel;
        auto it = stats.find(rule.channel);
        if (it == stats.end()) {
            status.healthy = false;
            status.message = "channel not found";
            status.severity = HealthSeverity::WARNING;
            return status;
        }
        status.actual_hz = it->second.avg_hz;
        if (rule.min_hz > 0 && it->second.avg_hz < rule.min_hz) {
            status.healthy = false;
            status.severity = (it->second.avg_hz > 0) ? HealthSeverity::WARNING : HealthSeverity::CRITICAL;
            status.message = "frequency below threshold: " + std::to_string(it->second.avg_hz) + "hz < " + std::to_string(rule.min_hz) + "hz";
        } else {
            status.healthy = true;
            status.message = "ok";
        }
        return status;
    }

    auto check_resource_with_monitor(int64_t pid, const ResourceHealthRule& rule) const -> ResourceHealthStatus {
        auto snap = res_monitor_->sample(pid);
        auto status = ResourceHealthStatus{};
        status.pid = snap.pid;
        status.name = snap.name;
        status.cpu_percent = snap.cpu_percent;
        status.rss_kb = snap.rss_kb;
        status.fd_count = snap.fd_count;

        const auto rss_mb = snap.rss_kb / 1024;

        if (rule.max_cpu_percent > 0 && snap.cpu_percent > rule.max_cpu_percent) {
            status.healthy = false;
            status.severity = std::max(status.severity, HealthSeverity::WARNING);
            status.message += "cpu=" + std::to_string(snap.cpu_percent) + "%>" + std::to_string(rule.max_cpu_percent) + "%; ";
        }
        if (rule.max_rss_mb > 0 && rss_mb > rule.max_rss_mb) {
            status.healthy = false;
            status.severity = std::max(status.severity, (rss_mb > rule.max_rss_mb * 2) ? HealthSeverity::CRITICAL : HealthSeverity::WARNING);
            status.message += "rss=" + std::to_string(rss_mb) + "MB>" + std::to_string(rule.max_rss_mb) + "MB; ";
        }
        if (rule.max_fd_count > 0 && snap.fd_count >= 0 && snap.fd_count > rule.max_fd_count) {
            status.healthy = false;
            status.severity = std::max(status.severity, HealthSeverity::WARNING);
            status.message += "fds=" + std::to_string(snap.fd_count) + ">" + std::to_string(rule.max_fd_count) + "; ";
        }

        if (status.healthy) {
            status.message = "ok";
        } else if (!status.message.empty() && status.message.back() == ' ') {
            status.message.pop_back();
            status.message.pop_back();
        }

        return status;
    }

    static auto check_resource_fallback(int64_t pid, const ResourceHealthRule&) -> ResourceHealthStatus {
        (void)pid;
        auto s = ResourceHealthStatus{};
        s.healthy = true;
        s.message = "no resource monitor (skipped)";
        return s;
    }

    static void build_summary(ComponentHealthReport* report) {
        if (report->overall_healthy) {
            report->summary_message = "ok";
            return;
        }
        std::string parts;
        if (!report->resource_status.healthy) {
            parts += "[resource] " + report->resource_status.message + "; ";
        }
        for (const auto& ts : report->channel_statuses) {
            if (!ts.healthy) {
                parts += "[channel:" + ts.channel + "] " + ts.message + "; ";
            }
        }
        report->summary_message = parts.empty() ? "unhealthy" : parts;
    }

    static auto getpid() -> int64_t { return static_cast<int64_t>(::getpid()); }

    std::shared_ptr<star::transport::MessageBus> bus_;
    std::shared_ptr<ResourceMonitor> res_monitor_;
    std::vector<ChannelHealthRule> channel_rules_;
    std::unordered_map<std::string, ComponentHealthRule> component_rules_;
    UnhealthyAction unhealthy_action_;
};

}  // namespace star::runtime::observability

#endif  // __BASEMENT_HEALTH_CHECKER_H__

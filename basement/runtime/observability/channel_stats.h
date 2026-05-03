#ifndef __STAR_RUNTIME_OBSERVABILITY_CHANNEL_STATS_H__
#define __STAR_RUNTIME_OBSERVABILITY_CHANNEL_STATS_H__

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace star::runtime::observability {

struct ChannelStatsSnapshot {
    std::string channel_name;
    int64_t total_messages{0};
    int64_t dropped_messages{0};
    double avg_payload_bytes{0.0};
    double min_payload_bytes{0.0};
    double max_payload_bytes{0.0};
    double avg_interval_us{0.0};
    double min_interval_us{0.0};
    double max_interval_us{0.0};
};

class ChannelStats {
  public:
    void record_write(size_t payload_bytes) {
        const auto now = steady_now_us();
        std::lock_guard<std::mutex> lock(mutex_);
        ++total_messages_;
        sum_payload_ += static_cast<double>(payload_bytes);
        min_payload_ = std::min(min_payload_, static_cast<double>(payload_bytes));
        max_payload_ = std::max(max_payload_, static_cast<double>(payload_bytes));
        if (last_write_time_ns_ > 0) {
            const auto interval = static_cast<double>(now - last_write_time_ns_) / 1000.0;
            sum_interval_ += interval;
            min_interval_ = std::min(min_interval_, interval);
            max_interval_ = std::max(max_interval_, interval);
        }
        last_write_time_ns_ = now;
    }

    void record_drop() { ++dropped_; }

    void record_read() { ++total_reads_; }

    auto snapshot(const std::string& channel_name) const -> ChannelStatsSnapshot {
        std::lock_guard<std::mutex> lock(mutex_);
        auto s = ChannelStatsSnapshot{};
        s.channel_name = channel_name;
        s.total_messages = total_messages_;
        s.dropped_messages = dropped_;
        if (total_messages_ > 0) {
            s.avg_payload_bytes = sum_payload_ / static_cast<double>(total_messages_);
            s.min_payload_bytes = min_payload_;
            s.max_payload_bytes = max_payload_;
        }
        const auto intervals = total_messages_ > 1 ? total_messages_ - 1 : 0;
        if (intervals > 0) {
            s.avg_interval_us = sum_interval_ / static_cast<double>(intervals);
            s.min_interval_us = min_interval_;
            s.max_interval_us = max_interval_;
        }
        return s;
    }

    auto message_count() const -> int64_t { return total_messages_.load(); }

    auto drop_count() const -> int64_t { return dropped_.load(); }

  private:
    static auto steady_now_us() -> int64_t { return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }

    mutable std::mutex mutex_;
    std::atomic<int64_t> total_messages_{0};
    std::atomic<int64_t> dropped_{0};
    std::atomic<int64_t> total_reads_{0};
    double sum_payload_{0.0};
    double min_payload_{1e18};
    double max_payload_{0.0};
    double sum_interval_{0.0};
    double min_interval_{1e18};
    double max_interval_{0.0};
    int64_t last_write_time_ns_{0};
};

class ObservabilityRegistry {
  public:
    auto get_or_create(const std::string& channel_name) -> std::shared_ptr<ChannelStats> {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stats_.find(channel_name);
        if (it != stats_.end()) {
            return it->second;
        }
        auto s = std::make_shared<ChannelStats>();
        stats_[channel_name] = s;
        return s;
    }

    auto snapshot_all() const -> std::vector<ChannelStatsSnapshot> {
        std::lock_guard<std::mutex> lock(mutex_);
        auto result = std::vector<ChannelStatsSnapshot>{};
        result.reserve(stats_.size());
        for (const auto& [channel_name, stats] : stats_) {
            result.push_back(stats->snapshot(channel_name));
        }
        return result;
    }

    auto dump_stats_lines() const -> std::vector<std::string> {
        auto snapshots = snapshot_all();
        auto lines = std::vector<std::string>{};
        lines.reserve(snapshots.size() + 1);
        lines.emplace_back("[Observability] channel | msgs | drops | avg_payload | avg_interval_us");
        for (const auto& s : snapshots) {
            lines.emplace_back("[Observability] " + s.channel_name + " | " + std::to_string(s.total_messages) + " | " + std::to_string(s.dropped_messages) + " | " +
                               std::to_string(static_cast<int64_t>(s.avg_payload_bytes)) + " | " + std::to_string(static_cast<int64_t>(s.avg_interval_us)));
        }
        return lines;
    }

  private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<ChannelStats>> stats_;
};

}  // namespace star::runtime::observability

#endif  // __STAR_RUNTIME_OBSERVABILITY_CHANNEL_STATS_H__

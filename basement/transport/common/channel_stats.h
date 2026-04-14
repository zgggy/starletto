#ifndef __TRANSPORT_CHANNEL_STATS_H__
#define __TRANSPORT_CHANNEL_STATS_H__

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace star::transport {

struct ChannelStats {
    uint64_t publish_count{0};
    uint64_t deliver_count{0};
    uint64_t dropped_count{0};
    uint64_t payload_bytes_total{0};
    uint64_t payload_bytes_last{0};
    uint64_t latency_total_us{0};
    uint64_t latency_max_us{0};
    uint64_t max_queue_depth{0};
    std::vector<uint64_t> latency_samples;
    std::chrono::steady_clock::time_point first_publish_tp{};
    std::chrono::steady_clock::time_point last_publish_tp{};

    static constexpr size_t kMaxLatencySamples = 1000;

    void record_publish(uint64_t payload_size) {
        const auto now = std::chrono::steady_clock::now();
        if (publish_count == 0) {
            first_publish_tp = now;
        }
        last_publish_tp = now;
        publish_count += 1;
        payload_bytes_total += payload_size;
        payload_bytes_last = payload_size;
    }

    void record_latency(uint64_t latency_us) {
        latency_total_us += latency_us;
        latency_max_us = std::max(latency_max_us, latency_us);
        if (latency_samples.size() < kMaxLatencySamples) {
            latency_samples.push_back(latency_us);
        }
    }

    void record_drop(uint64_t count) {
        dropped_count += count;
    }

    void record_deliver(uint64_t reader_count) {
        deliver_count += reader_count;
    }

    void update_queue_depth(size_t depth) {
        max_queue_depth = std::max(max_queue_depth, static_cast<uint64_t>(depth));
    }
};

struct ChannelStatsSnapshot {
    std::string channel_name;
    uint64_t publish_count{0};
    uint64_t deliver_count{0};
    uint64_t dropped_count{0};
    uint64_t payload_bytes_total{0};
    uint64_t payload_bytes_last{0};
    uint64_t latency_avg_us{0};
    uint64_t latency_p99_us{0};
    uint64_t latency_p999_us{0};
    uint64_t latency_max_us{0};
    uint64_t max_queue_depth{0};
    double avg_hz{0.0};
};

inline auto compute_percentile(const std::vector<uint64_t>& samples, double p) -> uint64_t {
    if (samples.empty()) {
        return 0;
    }
    auto sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    const auto idx = static_cast<size_t>(std::ceil(p * sorted.size())) - 1;
    return sorted[std::min(idx, sorted.size() - 1)];
}

inline auto compute_hz(const ChannelStats& stats) -> double {
    if (stats.publish_count <= 1) {
        return static_cast<double>(stats.publish_count);
    }
    const auto seconds = std::chrono::duration<double>(stats.last_publish_tp - stats.first_publish_tp).count();
    if (seconds <= 0.0) {
        return static_cast<double>(stats.publish_count);
    }
    return static_cast<double>(stats.publish_count - 1) / seconds;
}

inline auto compute_avg_latency_us(const ChannelStats& stats) -> uint64_t {
    const auto divisor = stats.publish_count > 0 ? stats.publish_count : 1;
    return stats.latency_total_us / divisor;
}

inline auto format_rate(double hz) -> std::string {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%.2f", hz);
    return std::string(buf);
}

inline auto make_stats_snapshot(const std::string& channel_name, const ChannelStats& stats) -> ChannelStatsSnapshot {
    return ChannelStatsSnapshot{
        .channel_name = channel_name,
        .publish_count = stats.publish_count,
        .deliver_count = stats.deliver_count,
        .dropped_count = stats.dropped_count,
        .payload_bytes_total = stats.payload_bytes_total,
        .payload_bytes_last = stats.payload_bytes_last,
        .latency_avg_us = compute_avg_latency_us(stats),
        .latency_p99_us = compute_percentile(stats.latency_samples, 0.99),
        .latency_p999_us = compute_percentile(stats.latency_samples, 0.999),
        .latency_max_us = stats.latency_max_us,
        .max_queue_depth = stats.max_queue_depth,
        .avg_hz = compute_hz(stats),
    };
}

inline auto format_stats_line(const std::string& transport_tag, const std::string& channel_name, const ChannelStats& stats) -> std::string {
    const auto hz = compute_hz(stats);
    const auto p99 = compute_percentile(stats.latency_samples, 0.99);
    const auto p999 = compute_percentile(stats.latency_samples, 0.999);
    return "transport." + transport_tag + " channel=" + channel_name + " publish=" + std::to_string(stats.publish_count) + " deliver=" + std::to_string(stats.deliver_count) +
           " dropped=" + std::to_string(stats.dropped_count) + " payload_bytes_total=" + std::to_string(stats.payload_bytes_total) +
           " payload_bytes_last=" + std::to_string(stats.payload_bytes_last) + " latency_avg_us=" + std::to_string(compute_avg_latency_us(stats)) + " latency_p99_us=" + std::to_string(p99) +
           " latency_p999_us=" + std::to_string(p999) + " latency_max_us=" + std::to_string(stats.latency_max_us) + " max_queue_depth=" + std::to_string(stats.max_queue_depth) +
           " avg_hz=" + format_rate(hz);
}

}  // namespace star::transport

#endif  // __TRANSPORT_CHANNEL_STATS_H__

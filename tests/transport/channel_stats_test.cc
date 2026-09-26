#include "basement/transport/common/channel_stats.h"

#include "tests/support/star_test.h"

namespace {

using star::transport::ChannelStats;
using star::transport::compute_avg_latency_us;
using star::transport::compute_hz;
using star::transport::format_stats_line;
using star::transport::make_stats_snapshot;

TEST_CASE("ChannelStats accumulates writes and payload bytes") {
    auto stats = ChannelStats{};
    stats.record_write(100);
    stats.record_write(250);

    REQUIRE_EQ(stats.write_count, 2ULL);
    REQUIRE_EQ(stats.payload_bytes_total, 350ULL);
    REQUIRE_EQ(stats.payload_bytes_last, 250ULL);
}

TEST_CASE("ChannelStats accumulates deliver and drop counters") {
    auto stats = ChannelStats{};
    stats.record_deliver(3);
    stats.record_deliver(1);
    stats.record_drop(2);

    REQUIRE_EQ(stats.deliver_count, 4ULL);
    REQUIRE_EQ(stats.dropped_count, 2ULL);
}

TEST_CASE("ChannelStats tracks max queue depth") {
    auto stats = ChannelStats{};
    stats.update_queue_depth(4);
    stats.update_queue_depth(2);
    stats.update_queue_depth(9);

    REQUIRE_EQ(stats.max_queue_depth, 9ULL);
}

TEST_CASE("ChannelStats latency average and max") {
    auto stats = ChannelStats{};
    stats.record_write(1);
    stats.record_latency(10);
    stats.record_write(1);
    stats.record_latency(30);

    REQUIRE_EQ(stats.latency_max_us, 30ULL);
    REQUIRE_EQ(stats.latency_total_us, 40ULL);
    REQUIRE_EQ(stats.latency_count, 2ULL);
    REQUIRE_EQ(compute_avg_latency_us(stats), 20ULL);
}

// P1-7 回归：平均值必须按延迟采样数求，而非 write_count——
// 二者并不必然相等（如按 reader 分别计延迟），用 write_count 会静默失真
TEST_CASE("ChannelStats latency average uses the latency sample count") {
    auto stats = ChannelStats{};
    stats.record_write(1);
    stats.record_latency(10);
    stats.record_latency(30);
    stats.record_latency(50);

    // 3 个延迟样本、1 次写入：平均应是 90/3=30，而不是 90/1=90
    REQUIRE_EQ(stats.latency_count, 3ULL);
    REQUIRE_EQ(compute_avg_latency_us(stats), 30ULL);
}

TEST_CASE("ChannelStats latency average is zero when no samples") {
    auto stats = ChannelStats{};
    stats.record_write(1);
    REQUIRE_EQ(compute_avg_latency_us(stats), 0ULL);
}

TEST_CASE("ChannelStats latency max tracks the largest sample") {
    auto stats = ChannelStats{};
    stats.record_latency(30);
    stats.record_latency(10);
    REQUIRE_EQ(stats.latency_max_us, 30ULL);
}

TEST_CASE("ChannelStats latency samples are capped") {
    auto stats = ChannelStats{};
    for (size_t i = 0; i < ChannelStats::kMaxLatencySamples + 100; ++i) {
        stats.record_latency(1);
    }
    REQUIRE_EQ(stats.latency_samples.size(), ChannelStats::kMaxLatencySamples);
}

TEST_CASE("ChannelStats snapshot reports percentiles and counters") {
    auto stats = ChannelStats{};
    for (uint64_t i = 1; i <= 100; ++i) {
        stats.record_write(i);
        stats.record_latency(i);
    }
    stats.record_deliver(1);
    stats.record_drop(3);
    stats.update_queue_depth(7);

    const auto snapshot = make_stats_snapshot("test.channel", stats);
    REQUIRE_EQ(snapshot.channel_name, std::string("test.channel"));
    REQUIRE_EQ(snapshot.write_count, 100ULL);
    REQUIRE_EQ(snapshot.deliver_count, 1ULL);
    REQUIRE_EQ(snapshot.dropped_count, 3ULL);
    REQUIRE_EQ(snapshot.payload_bytes_total, 5050ULL);
    REQUIRE_EQ(snapshot.payload_bytes_last, 100ULL);
    REQUIRE_EQ(snapshot.latency_max_us, 100ULL);
    REQUIRE_EQ(snapshot.max_queue_depth, 7ULL);
    CHECK(snapshot.latency_p99_us >= snapshot.latency_avg_us);
    CHECK(snapshot.latency_p999_us >= snapshot.latency_p99_us);
}

TEST_CASE("compute_hz handles degenerate write counts") {
    REQUIRE_EQ(compute_hz(ChannelStats{}), 0.0);

    auto single = ChannelStats{};
    single.record_write(1);
    REQUIRE_EQ(compute_hz(single), 1.0);

    // 两次写入落在同一时刻时退化为 write_count，否则应为正频率
    auto two = ChannelStats{};
    two.record_write(1);
    two.record_write(1);
    CHECK(compute_hz(two) > 0.0);
}

TEST_CASE("format_stats_line contains the monitoring key values") {
    auto stats = ChannelStats{};
    stats.record_write(64);
    stats.record_deliver(2);
    stats.record_latency(5);

    const auto line = format_stats_line("native", "planning.trajectory", stats);
    CHECK(line.find("transport.native") != std::string::npos);
    CHECK(line.find("channel=planning.trajectory") != std::string::npos);
    CHECK(line.find("write=1") != std::string::npos);
    CHECK(line.find("deliver=2") != std::string::npos);
    CHECK(line.find("payload_bytes_total=64") != std::string::npos);
    CHECK(line.find("latency_avg_us=") != std::string::npos);
    CHECK(line.find("avg_hz=") != std::string::npos);
}

}  // namespace

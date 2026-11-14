#include <unistd.h>

#include <chrono>
#include <string>

#include "basement/runtime/observability/resource_monitor.h"

#include "tests/support/star_test.h"

using star::runtime::observability::ResourceMonitor;

// 资源项以前在 macOS 上恒为 0（只有 /proc 实现）。这里断言它们真的采到了值。
TEST_CASE("resource monitor samples the current process") {
    auto monitor = ResourceMonitor{};
    const auto first = monitor.sample_self();

    REQUIRE_EQ(first.pid, static_cast<int64_t>(::getpid()));
    REQUIRE(first.rss_kb > 0);
    REQUIRE(first.thread_count > 0);
    REQUIRE(first.fd_count > 0);
    REQUIRE(not first.name.empty());
    REQUIRE(first.timestamp_ns > 0);

    const auto busy_until = std::chrono::steady_clock::now() + std::chrono::milliseconds(30);
    auto spin = 0.0;
    while (std::chrono::steady_clock::now() < busy_until) {
        spin += 1.0;
    }
    REQUIRE(spin > 0.0);

    // CPU 百分比需要两次采样做差分；第一次允许为 0，第二次必须非负且时间前进
    const auto second = monitor.sample_self();
    REQUIRE(second.timestamp_ns >= first.timestamp_ns);
    REQUIRE(second.cpu_percent >= 0.0);
    REQUIRE(not second.format_line().empty());
}

TEST_CASE("resource monitor returns an empty snapshot for an unknown pid") {
    auto monitor = ResourceMonitor{};
    const auto snapshot = monitor.sample(0);
    REQUIRE_EQ(snapshot.pid, int64_t{0});
    REQUIRE_EQ(snapshot.rss_kb, uint64_t{0});
    REQUIRE_EQ(snapshot.fd_count, 0);
    REQUIRE_EQ(snapshot.thread_count, int64_t{0});
}

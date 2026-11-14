#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/observability/health_monitor.h"
#include "basement/transport/native/native_message_bus.h"

#include "tests/support/star_test.h"

namespace {

using WorkerHealth = star::generated::observability::worker_health::WorkerHealth;
using star::runtime::observability::HealthMonitor;
using star::runtime::observability::kHealthChannelName;
using star::transport::BinaryPayloadView;
using star::transport::ChannelDescriptor;
using star::transport::MessageView;
using star::transport::NativeMessageBus;

auto make_health_descriptor() -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = kHealthChannelName;
    descriptor.type_name = "observability.worker_health";
    descriptor.schema_type_id = WorkerHealth::schema_type_id();
    return descriptor;
}

auto decode_health(const MessageView& message) -> WorkerHealth {
    if (const auto typed = message.payload.as<WorkerHealth>()) {
        return *typed;
    }
    if (const auto binary = message.payload.as<BinaryPayloadView>()) {
        return WorkerHealth::from_binary(binary->bytes());
    }
    return WorkerHealth{};
}

}  // namespace

// HealthChecker 此前只有单测；HealthMonitor 把它接到运行时并发布到普通通道
TEST_CASE("HealthMonitor publishes an unhealthy channel rule") {
    const auto bus = std::make_shared<NativeMessageBus>();
    auto mutex = std::mutex{};
    auto received = std::vector<WorkerHealth>{};
    const auto reader = bus->create_reader(make_health_descriptor(), [&](const MessageView& message) {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(decode_health(message));
    });

    auto rule = HealthMonitor::Rule{};
    rule.component = "probe";
    rule.has_channel = true;
    rule.channel.channel = "test.health.no_traffic";
    rule.channel.min_hz = 100.0;
    auto monitor = HealthMonitor{*bus, "test_group", std::vector<HealthMonitor::Rule>{rule}};

    const auto reports = monitor.evaluate_once();
    REQUIRE_EQ(reports.size(), 1U);
    REQUIRE(not reports[0].overall_healthy);

    std::lock_guard<std::mutex> lock(mutex);
    REQUIRE_EQ(received.size(), 1U);
    REQUIRE_EQ(received[0].component, std::string("probe"));
    REQUIRE(not received[0].healthy);
    REQUIRE_EQ(received[0].severity, std::string("warn"));
    REQUIRE_EQ(received[0].process_group.value_or(""), std::string("test_group"));
    REQUIRE_EQ(received[0].channel.value_or(""), std::string("test.health.no_traffic"));
}

TEST_CASE("HealthMonitor reports a component without channel rule as healthy") {
    const auto bus = std::make_shared<NativeMessageBus>();
    auto mutex = std::mutex{};
    auto received = std::vector<WorkerHealth>{};
    const auto reader = bus->create_reader(make_health_descriptor(), [&](const MessageView& message) {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(decode_health(message));
    });

    auto rule = HealthMonitor::Rule{};
    rule.component = "quiet";
    rule.has_channel = false;
    auto monitor = HealthMonitor{*bus, "test_group", std::vector<HealthMonitor::Rule>{rule}};

    const auto reports = monitor.evaluate_once();
    REQUIRE(reports[0].overall_healthy);
    REQUIRE_EQ(reports[0].max_severity, star::runtime::observability::HealthSeverity::OK);

    std::lock_guard<std::mutex> lock(mutex);
    REQUIRE_EQ(received.size(), 1U);
    REQUIRE(received[0].healthy);
    REQUIRE_EQ(received[0].severity, std::string("ok"));
}

TEST_CASE("HealthMonitor evaluates periodically once started") {
    const auto bus = std::make_shared<NativeMessageBus>();
    auto count = std::atomic<int>{0};
    const auto reader = bus->create_reader(make_health_descriptor(), [&](const MessageView&) {
        count.fetch_add(1, std::memory_order_relaxed);
    });

    auto rule = HealthMonitor::Rule{};
    rule.component = "periodic";
    auto monitor = HealthMonitor{*bus, "test_group", std::vector<HealthMonitor::Rule>{rule}, 50};
    monitor.start();
    for (int i = 0; i < 100 && count.load(std::memory_order_relaxed) < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    monitor.stop();
    REQUIRE(count.load(std::memory_order_relaxed) >= 2);
}

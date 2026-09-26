#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "basement/runtime/observability/health_checker.h"
#include "basement/runtime/observability/resource_monitor.h"
#include "basement/transport/native/native_message_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::runtime::observability::ChannelHealthRule;
using star::runtime::observability::ComponentHealthReport;
using star::runtime::observability::ComponentHealthRule;
using star::runtime::observability::HealthChecker;
using star::runtime::observability::HealthSeverity;
using star::runtime::observability::ResourceHealthRule;
using star::transport::ChannelDescriptor;
using star::transport::NativeMessageBus;

auto make_bus() -> std::shared_ptr<NativeMessageBus> { return std::make_shared<NativeMessageBus>(); }

auto make_channel(const std::string& name) -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = name;
    descriptor.type_name = "test.Msg";
    return descriptor;
}

// 写入恰好 1 条：compute_hz 的退化分支使其频率恒为 1hz，断言因此确定
// （多次写入的瞬时频率取决于写入间隔，不可用作断言）
void seed_single_write(const std::shared_ptr<NativeMessageBus>& bus, const std::string& name) {
    const auto descriptor = make_channel(name);
    const auto reader = bus->create_reader(descriptor, [](const star::transport::MessageView&) {});
    const auto writer = bus->create_writer(descriptor);
    auto view = star::transport::MessageView{};
    view.payload = star::base::TypedConstPtr::from(std::make_shared<const int>(1));
    (void)writer->write(view);
}

TEST_CASE("HealthChecker reports warning for a channel with no traffic") {
    const auto bus = make_bus();
    // 只声明不写入：统计不可见，应给出「channel not found」
    bus->declare_channel(make_channel("test.silent"));

    auto checker = HealthChecker{bus};
    checker.add_channel_rule(ChannelHealthRule{.channel = "test.silent", .min_hz = 1.0});

    const auto statuses = checker.check_all_channels();
    REQUIRE_EQ(statuses.size(), 1U);
    REQUIRE(not statuses.front().healthy);
    REQUIRE(statuses.front().severity == HealthSeverity::WARNING);
    CHECK(statuses.front().message.find("not found") != std::string::npos);
}

TEST_CASE("HealthChecker passes a channel meeting the frequency threshold") {
    const auto bus = make_bus();
    seed_single_write(bus, "test.ok");  // 1hz

    auto checker = HealthChecker{bus};
    checker.add_channel_rule(ChannelHealthRule{.channel = "test.ok", .min_hz = 1.0});

    const auto statuses = checker.check_all_channels();
    REQUIRE_EQ(statuses.size(), 1U);
    REQUIRE(statuses.front().healthy);
    REQUIRE(statuses.front().severity == HealthSeverity::OK);
    REQUIRE_EQ(statuses.front().actual_hz, 1.0);
}

TEST_CASE("HealthChecker warns when frequency falls below the threshold") {
    const auto bus = make_bus();
    seed_single_write(bus, "test.slow");  // 1hz < 5hz

    auto checker = HealthChecker{bus};
    checker.add_channel_rule(ChannelHealthRule{.channel = "test.slow", .min_hz = 5.0});

    const auto statuses = checker.check_all_channels();
    REQUIRE_EQ(statuses.size(), 1U);
    REQUIRE(not statuses.front().healthy);
    REQUIRE(statuses.front().severity == HealthSeverity::WARNING);
    CHECK(statuses.front().message.find("frequency below threshold") != std::string::npos);
}

TEST_CASE("HealthChecker only evaluates its registered rules") {
    const auto bus = make_bus();
    seed_single_write(bus, "test.monitored");
    seed_single_write(bus, "test.other");

    auto checker = HealthChecker{bus};
    checker.add_channel_rule(ChannelHealthRule{.channel = "test.monitored", .min_hz = 1.0});

    REQUIRE_EQ(checker.check_all_channels().size(), 1U);
}

TEST_CASE("HealthChecker without a resource monitor falls back to healthy") {
    const auto bus = make_bus();
    auto checker = HealthChecker{bus};
    // 无 monitor：check_resource 走 fallback，不依赖 /proc（macOS 上不存在）
    const auto status = checker.check_resource(::getpid(), ResourceHealthRule{});
    REQUIRE(status.healthy);
    CHECK(status.message.find("no resource monitor") != std::string::npos);
}

TEST_CASE("HealthChecker aggregates the component's own channel rule") {
    const auto bus = make_bus();
    seed_single_write(bus, "test.comp.ok");

    auto checker = HealthChecker{bus};
    auto rule = ComponentHealthRule{};
    rule.name = "my_component";
    rule.channel_rule = ChannelHealthRule{.channel = "test.comp.ok", .min_hz = 1.0};
    checker.add_component_rule(rule);

    // 回归：check_component 曾遍历全局 channel_rules_，忽略组件自己的规则
    const auto report = checker.check_component("my_component", ::getpid());
    REQUIRE(report.overall_healthy);
    REQUIRE_EQ(report.component_name, std::string("my_component"));
    REQUIRE_EQ(report.channel_statuses.size(), 1U);
    REQUIRE_EQ(report.channel_statuses.front().channel, std::string("test.comp.ok"));
    CHECK(report.summary_message == "ok");
}

TEST_CASE("HealthChecker component report is unhealthy when its channel is unhealthy") {
    const auto bus = make_bus();
    bus->declare_channel(make_channel("test.comp.silent"));

    auto checker = HealthChecker{bus};
    auto rule = ComponentHealthRule{};
    rule.name = "my_component";
    rule.channel_rule = ChannelHealthRule{.channel = "test.comp.silent", .min_hz = 1.0};
    checker.add_component_rule(rule);

    const auto report = checker.check_component("my_component", ::getpid());
    REQUIRE(not report.overall_healthy);
    REQUIRE_EQ(report.channel_statuses.size(), 1U);
    CHECK(report.summary_message.find("channel") != std::string::npos);
}

TEST_CASE("HealthChecker invokes the unhealthy action on a failing report") {
    const auto bus = make_bus();
    bus->declare_channel(make_channel("test.action.silent"));

    auto checker = HealthChecker{bus};
    auto rule = ComponentHealthRule{};
    rule.name = "my_component";
    rule.channel_rule = ChannelHealthRule{.channel = "test.action.silent", .min_hz = 1.0};
    checker.add_component_rule(rule);

    auto triggered = 0;
    checker.set_unhealthy_action([&](const ComponentHealthReport&) { ++triggered; });

    (void)checker.check_component("my_component", ::getpid());
    REQUIRE_EQ(triggered, 1);
}

}  // namespace

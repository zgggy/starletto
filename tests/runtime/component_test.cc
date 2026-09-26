#include <chrono>
#include <cstring>
#include <stdexcept>
#include <memory>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/component/component.h"
#include "basement/runtime/factory/runtime_factory.h"

#include "tests/support/star_test.h"

namespace {

using star::generated::runtime::component_load_plan::ComponentLoadPlan;
using star::generated::runtime::component_load_plan::PortPlan;
using star::runtime::Component;
using star::runtime::RuntimeFactory;

// 满足 Component 对消息类型的契约：encode_binary() + 静态 from_binary()
struct TestMsg {
    int value{0};

    auto encode_binary() const -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>(sizeof(int));
        std::memcpy(out.data(), &value, sizeof(int));
        return out;
    }

    static auto from_binary(std::span<const uint8_t> bytes) -> TestMsg {
        auto msg = TestMsg{};
        if (bytes.size() >= sizeof(int)) {
            std::memcpy(&msg.value, bytes.data(), sizeof(int));
        }
        return msg;
    }
};

class LifecycleProbe final : public Component {
  public:
    void init() override { events.push_back("init"); }

    auto process_once() -> bool override {
        events.push_back("process");
        return true;
    }

    void shutdown() override { events.push_back("shutdown"); }

    std::vector<std::string> events;
};

auto make_plan(const std::string& channel) -> ComponentLoadPlan {
    auto plan = ComponentLoadPlan{};
    plan.module_name = "test";
    plan.component_name = "probe";
    plan.component_class = "star::test::LifecycleProbe";

    auto input = PortPlan{};
    input.name = "in";
    input.channel = channel;
    plan.inputs = std::vector<PortPlan>{input};

    auto output = PortPlan{};
    output.name = "out";
    output.channel = channel;
    plan.outputs = std::vector<PortPlan>{output};

    return plan;
}

TEST_CASE("Component lifecycle hooks run in order") {
    auto probe = LifecycleProbe{};
    probe.init();
    REQUIRE(probe.process_once());
    probe.shutdown();

    REQUIRE_EQ(probe.events.size(), 3U);
    REQUIRE_EQ(probe.events[0], std::string("init"));
    REQUIRE_EQ(probe.events[1], std::string("process"));
    REQUIRE_EQ(probe.events[2], std::string("shutdown"));
}

TEST_CASE("Component base class defaults are inert") {
    auto component = Component{};
    component.init();
    REQUIRE(component.process_once());
    component.shutdown();
    REQUIRE(component.load_plan() == nullptr);
    REQUIRE(component.context() == nullptr);
}

TEST_CASE("Component context roundtrip") {
    const auto context = RuntimeFactory::create_native_context();
    REQUIRE(context != nullptr);

    auto probe = LifecycleProbe{};
    REQUIRE(probe.context() == nullptr);
    probe.set_context(context);
    REQUIRE(probe.context() == context);
}

TEST_CASE("Component load plan roundtrip") {
    auto probe = LifecycleProbe{};
    REQUIRE(probe.load_plan() == nullptr);

    const auto plan = make_plan("test.component.roundtrip");
    probe.set_load_plan(plan);

    const auto* loaded = probe.load_plan();
    REQUIRE(loaded != nullptr);
    REQUIRE_EQ(loaded->component_name, std::string("probe"));
    REQUIRE(loaded->outputs.has_value());
    REQUIRE_EQ(loaded->outputs->size(), 1U);
}

TEST_CASE("Component auto_configure delivers written messages to its own reader") {
    const auto context = RuntimeFactory::create_native_context();
    auto probe = LifecycleProbe{};
    probe.set_context(context);
    probe.set_load_plan(make_plan("test.component.loopback"));
    probe.auto_configure();

    // 输入与输出挂在同一通道上，形成组件内回环
    const auto message = TestMsg{42};
    probe.write("out", message);

    // 投递在总线回调中完成，稍等一拍
    for (int i = 0; i < 100; ++i) {
        const auto received = probe.read_latest<TestMsg>("in");
        if (received) {
            REQUIRE_EQ(received->value, 42);
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw star::test::Failure{"Component auto_configure 回环未在超时内收到消息"};
}

// 编码失败属运行时状况：应以状态码返回，不得抛异常（否则又被调度层降级为日志）
struct BadEncoderMsg {
    int value{0};

    auto encode_binary() const -> std::vector<uint8_t> { throw std::runtime_error("encode boom"); }

    static auto from_binary(std::span<const uint8_t> bytes) -> BadEncoderMsg {
        (void)bytes;
        return {};
    }
};

TEST_CASE("Component write returns a status code on encode failure instead of throwing") {
    // shm 路径会真正执行编码；inproc 的序列化器是惰性调用（仅录制/跨进程时触发）
    const auto context = RuntimeFactory::create_native_context("shm");
    auto probe = LifecycleProbe{};
    probe.set_context(context);
    probe.set_load_plan(make_plan("test.component.encodefail"));
    probe.auto_configure();

    auto result = star::transport::WriteResult{};
    auto threw = false;
    try {
        result = probe.write("out", BadEncoderMsg{1});
    } catch (...) {
        threw = true;
    }

    REQUIRE(not threw);
    REQUIRE(not result.ok());
}

TEST_CASE("Component write returns ok status on success") {
    const auto context = RuntimeFactory::create_native_context();
    auto probe = LifecycleProbe{};
    probe.set_context(context);
    probe.set_load_plan(make_plan("test.component.writestatus"));
    probe.auto_configure();

    const auto result = probe.write("out", TestMsg{5});
    REQUIRE(result.ok());
}

TEST_CASE("Component write to an undeclared port throws") {
    const auto context = RuntimeFactory::create_native_context();
    auto probe = LifecycleProbe{};
    probe.set_context(context);
    probe.set_load_plan(make_plan("test.component.undeclared"));
    probe.auto_configure();

    auto threw = false;
    try {
        probe.write("nope", TestMsg{1});
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("Component read_latest returns nothing for unknown port") {
    const auto context = RuntimeFactory::create_native_context();
    auto probe = LifecycleProbe{};
    probe.set_context(context);
    probe.set_load_plan(make_plan("test.component.unknown"));
    probe.auto_configure();

    REQUIRE(probe.read_latest<TestMsg>("missing") == nullptr);
}

TEST_CASE("Component auto_configure without plan or context is a no-op") {
    auto probe = LifecycleProbe{};
    probe.auto_configure();
    REQUIRE(probe.read_latest<TestMsg>("in") == nullptr);
}

}  // namespace

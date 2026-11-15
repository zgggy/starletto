#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/component/frame_inputs.h"
#include "basement/runtime/trigger/trigger.h"
#include "basement/transport/api/message_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::runtime::FrameInputs;
using star::runtime::SyncTrigger;
using star::runtime::TimerTrigger;
using star::transport::MessageView;

TEST_CASE("SyncTrigger ignores channels that are not required") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](std::shared_ptr<const FrameInputs> inputs) { fired.push_back(inputs ? inputs->frame_id : -1); }};
    trigger.add_required_channel("a", "a");

    trigger.on_message("unknown", MessageView{});
    trigger.on_message("another", MessageView{});

    REQUIRE(fired.empty());
    REQUIRE_EQ(trigger.frame_id(), 0);
}

TEST_CASE("SyncTrigger fires only after every required channel arrives") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](std::shared_ptr<const FrameInputs> inputs) { fired.push_back(inputs ? inputs->frame_id : -1); }};
    trigger.add_required_channel("a", "a");
    trigger.add_required_channel("b", "b");

    trigger.on_message("a", MessageView{});
    REQUIRE(fired.empty());
    REQUIRE_EQ(trigger.frame_id(), 0);

    trigger.on_message("b", MessageView{});
    REQUIRE_EQ(fired.size(), 1U);
    REQUIRE_EQ(fired.front(), 1);
    REQUIRE_EQ(trigger.frame_id(), 1);
}

TEST_CASE("SyncTrigger deduplicates repeated messages on one channel") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](std::shared_ptr<const FrameInputs> inputs) { fired.push_back(inputs ? inputs->frame_id : -1); }};
    trigger.add_required_channel("a", "a");
    trigger.add_required_channel("b", "b");

    // 同一通道重复到达不应在 b 缺席时凑齐条件
    trigger.on_message("a", MessageView{});
    trigger.on_message("a", MessageView{});
    trigger.on_message("a", MessageView{});
    REQUIRE(fired.empty());

    trigger.on_message("b", MessageView{});
    REQUIRE_EQ(fired.size(), 1U);
}

TEST_CASE("SyncTrigger frame ids are strictly increasing across sync events") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](std::shared_ptr<const FrameInputs> inputs) { fired.push_back(inputs ? inputs->frame_id : -1); }};
    trigger.add_required_channel("a", "a");
    trigger.add_required_channel("b", "b");

    for (int i = 0; i < 5; ++i) {
        trigger.on_message("a", MessageView{});
        trigger.on_message("b", MessageView{});
    }

    REQUIRE_EQ(fired.size(), 5U);
    REQUIRE_EQ(fired[0], 1);
    REQUIRE_EQ(fired[4], 5);
    REQUIRE_EQ(trigger.frame_id(), 5);
}

TEST_CASE("SyncTrigger carries the triggering messages in FrameInputs") {
    auto captured = std::shared_ptr<const FrameInputs>{};
    auto trigger = SyncTrigger{[&](std::shared_ptr<const FrameInputs> inputs) { captured = std::move(inputs); }};
    trigger.add_required_channel("a", "port_a");
    trigger.add_required_channel("b", "port_b");

    auto make_message = [](uint8_t value) {
        auto owner = std::make_shared<std::vector<uint8_t>>(1, value);
        auto view = MessageView{};
        view.payload = star::base::TypedConstPtr::from(
            std::make_shared<star::transport::BinaryPayloadView>(star::transport::BinaryPayloadView{owner, owner->data(), owner->size()}));
        return view;
    };

    trigger.on_message("a", make_message(0x11));
    REQUIRE(captured == nullptr);
    trigger.on_message("b", make_message(0x22));

    REQUIRE(captured != nullptr);
    REQUIRE_EQ(captured->messages.size(), 2U);
    const auto* a = captured->find("port_a");
    const auto* b = captured->find("port_b");
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    const auto a_bytes = a->payload.as<star::transport::BinaryPayloadView>();
    const auto b_bytes = b->payload.as<star::transport::BinaryPayloadView>();
    REQUIRE(a_bytes != nullptr);
    REQUIRE(b_bytes != nullptr);
    REQUIRE_EQ(a_bytes->bytes().front(), uint8_t{0x11});
    REQUIRE_EQ(b_bytes->bytes().front(), uint8_t{0x22});
}

TEST_CASE("SyncTrigger required_count reflects registered channels") {
    auto trigger = SyncTrigger{[](std::shared_ptr<const FrameInputs>) {}};
    REQUIRE_EQ(trigger.required_count(), 0U);
    trigger.add_required_channel("a", "a");
    trigger.add_required_channel("b", "b");
    trigger.add_required_channel("a", "a");
    REQUIRE_EQ(trigger.required_count(), 2U);
}

TEST_CASE("SyncTrigger stop discards partially arrived frames") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](std::shared_ptr<const FrameInputs> inputs) { fired.push_back(inputs ? inputs->frame_id : -1); }};
    trigger.add_required_channel("a", "a");
    trigger.add_required_channel("b", "b");

    trigger.on_message("a", MessageView{});
    trigger.stop();

    // stop 后补上 b 不应触发：a 的到达记录已被清空
    trigger.on_message("b", MessageView{});
    REQUIRE(fired.empty());

    trigger.on_message("a", MessageView{});
    REQUIRE_EQ(fired.size(), 1U);
}

TEST_CASE("TimerTrigger exposes its cycle") {
    auto trigger = TimerTrigger{[](std::shared_ptr<const FrameInputs>) {}, 25.0};
    REQUIRE_EQ(trigger.cycle_ms(), 25.0);
}

TEST_CASE("TimerTrigger fires repeatedly with increasing frame ids") {
    auto fired = std::vector<int64_t>{};
    auto mutex = std::mutex{};
    auto trigger = TimerTrigger{
        [&](std::shared_ptr<const FrameInputs> inputs) {
            std::lock_guard<std::mutex> lock(mutex);
            fired.push_back(inputs ? inputs->frame_id : -1);
        },
        5.0,
    };

    trigger.start();
    // run_loop 会立即触发一次，之后按周期触发
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (fired.size() >= 3) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    trigger.stop();

    std::lock_guard<std::mutex> lock(mutex);
    REQUIRE(fired.size() >= 3);
    for (size_t i = 1; i < fired.size(); ++i) {
        REQUIRE(fired[i] > fired[i - 1]);
    }
    REQUIRE_EQ(fired.front(), 1);
}

TEST_CASE("TimerTrigger stop is idempotent and restart keeps frame ids monotonic") {
    auto trigger = TimerTrigger{[](std::shared_ptr<const FrameInputs>) {}, 5.0};

    trigger.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    trigger.stop();
    trigger.stop();  // 重复 stop 幂等

    const auto after_first_run = trigger.frame_id();
    REQUIRE(after_first_run > 0);

    // 重启后 frame_id 不重置，继续单调递增
    trigger.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    trigger.stop();
    REQUIRE(trigger.frame_id() > after_first_run);
}

// P2-1 回归测试：回调重入触发器的加锁方法不得死锁。
// 回调曾经在持有 mutex_ 时被调用，重入 on_message()/frame_id() 即互斥死锁；
// 此处用 async + 超时把死锁变成干净的失败而非挂死。
TEST_CASE("SyncTrigger callback may re-enter locking methods without deadlock") {
    std::unique_ptr<SyncTrigger> trigger;
    auto reentered = std::atomic<int>{0};
    trigger = std::make_unique<SyncTrigger>([&](std::shared_ptr<const FrameInputs> inputs) {
        // 回调里调用会加 mutex_ 的查询方法
        (void)trigger->frame_id();
        (void)trigger->required_count();
        REQUIRE_EQ(trigger->frame_id(), inputs ? inputs->frame_id : -1);
        reentered.fetch_add(1);
    });
    trigger->add_required_channel("a", "a");

    auto future = std::async(std::launch::async, [&]() { trigger->on_message("a", MessageView{}); });
    REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE_EQ(reentered.load(), 1);
    REQUIRE_EQ(trigger->frame_id(), 1);
}

TEST_CASE("SyncTrigger callback may re-enter on_message without deadlock") {
    std::unique_ptr<SyncTrigger> trigger;
    auto in_callback = std::atomic<bool>{false};
    auto reentered = std::atomic<int>{0};
    trigger = std::make_unique<SyncTrigger>([&](std::shared_ptr<const FrameInputs>) {
        if (in_callback.exchange(true)) {
            return;  // 防止回调内再次触发导致递归
        }
        // 回调里再次投递（同样会加 mutex_）；"ping" 非必需通道，命中早退分支
        trigger->on_message("ping", MessageView{});
        reentered.fetch_add(1);
    });
    trigger->add_required_channel("a", "a");

    auto future = std::async(std::launch::async, [&]() { trigger->on_message("a", MessageView{}); });
    REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE_EQ(reentered.load(), 1);
}

}  // namespace

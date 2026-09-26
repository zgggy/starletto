#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/trigger/trigger.h"

#include "tests/support/star_test.h"

namespace {

using star::runtime::SyncTrigger;
using star::runtime::TimerTrigger;

TEST_CASE("SyncTrigger ignores channels that are not required") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](int64_t frame_id) { fired.push_back(frame_id); }};
    trigger.add_required_channel("a");

    trigger.on_message("unknown");
    trigger.on_message("another");

    REQUIRE(fired.empty());
    REQUIRE_EQ(trigger.frame_id(), 0);
}

TEST_CASE("SyncTrigger fires only after every required channel arrives") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](int64_t frame_id) { fired.push_back(frame_id); }};
    trigger.add_required_channel("a");
    trigger.add_required_channel("b");

    trigger.on_message("a");
    REQUIRE(fired.empty());
    REQUIRE_EQ(trigger.frame_id(), 0);

    trigger.on_message("b");
    REQUIRE_EQ(fired.size(), 1U);
    REQUIRE_EQ(fired.front(), 1);
    REQUIRE_EQ(trigger.frame_id(), 1);
}

TEST_CASE("SyncTrigger deduplicates repeated messages on one channel") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](int64_t frame_id) { fired.push_back(frame_id); }};
    trigger.add_required_channel("a");
    trigger.add_required_channel("b");

    // 同一通道重复到达不应在 b 缺席时凑齐条件
    trigger.on_message("a");
    trigger.on_message("a");
    trigger.on_message("a");
    REQUIRE(fired.empty());

    trigger.on_message("b");
    REQUIRE_EQ(fired.size(), 1U);
}

TEST_CASE("SyncTrigger frame ids are strictly increasing across sync events") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](int64_t frame_id) { fired.push_back(frame_id); }};
    trigger.add_required_channel("a");
    trigger.add_required_channel("b");

    for (int i = 0; i < 5; ++i) {
        trigger.on_message("a");
        trigger.on_message("b");
    }

    REQUIRE_EQ(fired.size(), 5U);
    REQUIRE_EQ(fired[0], 1);
    REQUIRE_EQ(fired[4], 5);
    REQUIRE_EQ(trigger.frame_id(), 5);
}

TEST_CASE("SyncTrigger required_count reflects registered channels") {
    auto trigger = SyncTrigger{[](int64_t) {}};
    REQUIRE_EQ(trigger.required_count(), 0U);
    trigger.add_required_channel("a");
    trigger.add_required_channel("b");
    trigger.add_required_channel("a");
    REQUIRE_EQ(trigger.required_count(), 2U);
}

TEST_CASE("SyncTrigger stop discards partially arrived frames") {
    auto fired = std::vector<int64_t>{};
    auto trigger = SyncTrigger{[&](int64_t frame_id) { fired.push_back(frame_id); }};
    trigger.add_required_channel("a");
    trigger.add_required_channel("b");

    trigger.on_message("a");
    trigger.stop();

    // stop 后补上 b 不应触发：a 的到达记录已被清空
    trigger.on_message("b");
    REQUIRE(fired.empty());

    trigger.on_message("a");
    REQUIRE_EQ(fired.size(), 1U);
}

TEST_CASE("TimerTrigger exposes its cycle") {
    auto trigger = TimerTrigger{[](int64_t) {}, 25.0};
    REQUIRE_EQ(trigger.cycle_ms(), 25.0);
}

TEST_CASE("TimerTrigger fires repeatedly with increasing frame ids") {
    auto fired = std::vector<int64_t>{};
    auto mutex = std::mutex{};
    auto trigger = TimerTrigger{
        [&](int64_t frame_id) {
            std::lock_guard<std::mutex> lock(mutex);
            fired.push_back(frame_id);
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
    auto trigger = TimerTrigger{[](int64_t) {}, 5.0};

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
    trigger = std::make_unique<SyncTrigger>([&](int64_t frame_id) {
        // 回调里调用会加 mutex_ 的查询方法
        (void)trigger->frame_id();
        (void)trigger->required_count();
        REQUIRE_EQ(trigger->frame_id(), frame_id);
        reentered.fetch_add(1);
    });
    trigger->add_required_channel("a");

    auto future = std::async(std::launch::async, [&]() { trigger->on_message("a"); });
    REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE_EQ(reentered.load(), 1);
    REQUIRE_EQ(trigger->frame_id(), 1);
}

TEST_CASE("SyncTrigger callback may re-enter on_message without deadlock") {
    std::unique_ptr<SyncTrigger> trigger;
    auto in_callback = std::atomic<bool>{false};
    auto reentered = std::atomic<int>{0};
    trigger = std::make_unique<SyncTrigger>([&](int64_t) {
        if (in_callback.exchange(true)) {
            return;  // 防止回调内再次触发导致递归
        }
        // 回调里再次投递（同样会加 mutex_）；"ping" 非必需通道，命中早退分支
        trigger->on_message("ping");
        reentered.fetch_add(1);
    });
    trigger->add_required_channel("a");

    auto future = std::async(std::launch::async, [&]() { trigger->on_message("a"); });
    REQUIRE(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    REQUIRE_EQ(reentered.load(), 1);
}

}  // namespace

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/scheduler/native_scheduler.h"

#include "tests/support/star_test.h"

namespace {

using star::runtime::NativeScheduler;
using star::runtime::SchedulerConfig;
using star::runtime::TaskDescriptor;

struct Recorder {
    mutable std::mutex mutex;
    std::vector<int64_t> frames;

    auto callback() -> std::function<bool(int64_t)> {
        return [this](int64_t frame_id) {
            std::lock_guard<std::mutex> lock(mutex);
            frames.push_back(frame_id);
            return true;
        };
    }

    auto size() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex);
        return frames.size();
    }

    auto snapshot() const -> std::vector<int64_t> {
        std::lock_guard<std::mutex> lock(mutex);
        return frames;
    }
};

auto make_task(const std::string& name, std::function<bool(int64_t)> callback) -> TaskDescriptor {
    auto descriptor = TaskDescriptor{};
    descriptor.name = name;
    descriptor.callback = std::move(callback);
    return descriptor;
}

void wait_for_count(const Recorder& recorder, size_t expected) {
    for (int i = 0; i < 200 and recorder.size() < expected; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

TEST_CASE("NativeScheduler rejects invalid task registration") {
    auto scheduler = NativeScheduler{};

    auto threw = false;
    try {
        scheduler.register_task(make_task("", [](int64_t) { return true; }));
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);

    threw = false;
    try {
        scheduler.register_task(make_task("no-callback", nullptr));
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("NativeScheduler delivers notified frame ids to the task callback") {
    auto scheduler = NativeScheduler{};
    scheduler.configure(SchedulerConfig{1});

    auto recorder = Recorder{};
    scheduler.register_task(make_task("probe", recorder.callback()));
    scheduler.start();

    scheduler.notify("probe", 1);
    scheduler.notify("probe", 2);
    scheduler.notify("probe", 3);

    wait_for_count(recorder, 3);
    scheduler.stop();

    const auto frames = recorder.snapshot();
    REQUIRE_EQ(frames.size(), 3U);
    REQUIRE_EQ(frames[0], 1);
    REQUIRE_EQ(frames[1], 2);
    REQUIRE_EQ(frames[2], 3);
}

// 帧一致性（basement_runtime_scheduler §5.1）：frame_id 严格单调递增
TEST_CASE("NativeScheduler preserves frame id order across many notifications") {
    auto scheduler = NativeScheduler{};
    scheduler.configure(SchedulerConfig{1});

    auto recorder = Recorder{};
    scheduler.register_task(make_task("probe", recorder.callback()));
    scheduler.start();

    constexpr int64_t kFrames = 200;
    for (int64_t frame_id = 1; frame_id <= kFrames; ++frame_id) {
        scheduler.notify("probe", frame_id);
    }

    wait_for_count(recorder, static_cast<size_t>(kFrames));
    scheduler.stop();

    const auto frames = recorder.snapshot();
    REQUIRE_EQ(frames.size(), static_cast<size_t>(kFrames));
    for (size_t i = 0; i < frames.size(); ++i) {
        REQUIRE_EQ(frames[i], static_cast<int64_t>(i + 1));
    }
}

TEST_CASE("NativeScheduler flushes notifications queued before start") {
    auto scheduler = NativeScheduler{};
    scheduler.configure(SchedulerConfig{1});

    auto recorder = Recorder{};
    scheduler.register_task(make_task("probe", recorder.callback()));

    // 未启动时 notify 应先排队，启动后统一冲刷
    scheduler.notify("probe", 7);
    scheduler.notify("probe", 8);
    REQUIRE_EQ(recorder.size(), 0U);

    scheduler.start();
    wait_for_count(recorder, 2);
    scheduler.stop();

    const auto frames = recorder.snapshot();
    REQUIRE_EQ(frames.size(), 2U);
    REQUIRE_EQ(frames[0], 7);
    REQUIRE_EQ(frames[1], 8);
}

TEST_CASE("NativeScheduler start and stop are idempotent") {
    auto scheduler = NativeScheduler{};
    scheduler.configure(SchedulerConfig{1});

    auto recorder = Recorder{};
    scheduler.register_task(make_task("probe", recorder.callback()));

    scheduler.start();
    scheduler.start();
    scheduler.stop();
    scheduler.stop();

    REQUIRE_EQ(recorder.size(), 0U);
}

// 逃逸到调度层的异常应计入 failed 统计，而非只留日志
TEST_CASE("NativeScheduler counts failed invocations") {
    auto scheduler = NativeScheduler{};
    scheduler.configure(SchedulerConfig{1});

    auto recorder = Recorder{};
    auto task = make_task("flaky", recorder.callback());
    task.callback = [&recorder](int64_t frame_id) -> bool {
        std::lock_guard<std::mutex> lock(recorder.mutex);
        recorder.frames.push_back(frame_id);
        throw std::runtime_error("boom");
    };
    scheduler.register_task(task);
    scheduler.start();

    scheduler.notify("flaky", 1);
    scheduler.notify("flaky", 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    scheduler.stop();

    REQUIRE_EQ(recorder.size(), 2U);
    REQUIRE_EQ(scheduler.failed_invocation_count("flaky"), 2ULL);
    REQUIRE_EQ(scheduler.total_invocation_count("flaky"), 2ULL);
}

TEST_CASE("NativeScheduler runs distinct tasks independently") {
    auto scheduler = NativeScheduler{};
    scheduler.configure(SchedulerConfig{2});

    auto first = Recorder{};
    auto second = Recorder{};
    scheduler.register_task(make_task("first", first.callback()));
    scheduler.register_task(make_task("second", second.callback()));
    scheduler.start();

    scheduler.notify("first", 1);
    scheduler.notify("second", 1);
    scheduler.notify("first", 2);
    scheduler.notify("second", 2);

    wait_for_count(first, 2);
    wait_for_count(second, 2);
    scheduler.stop();

    REQUIRE_EQ(first.size(), 2U);
    REQUIRE_EQ(second.size(), 2U);
}

}  // namespace

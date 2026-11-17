#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/factory/runtime_factory.h"
#include "basement/runtime/observability/stats_publisher.h"
#include "basement/schema/builtin/observability/worker_stats.s.h"

#include "tests/support/star_test.h"

namespace {

using WorkerStats = star::generated::observability::worker_stats::WorkerStats;
using star::runtime::observability::kStatsChannelName;
using star::runtime::observability::StatsPublisher;
using star::transport::BinaryPayloadView;
using star::transport::ChannelDescriptor;
using star::transport::MessageView;

auto make_stats_descriptor() -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = kStatsChannelName;
    descriptor.type_name = "observability.worker_stats";
    descriptor.schema_type_id = WorkerStats::schema_type_id();
    return descriptor;
}

auto make_binary_message(std::vector<uint8_t> bytes) -> MessageView {
    auto owner = std::make_shared<std::vector<uint8_t>>(std::move(bytes));
    auto payload = std::make_shared<BinaryPayloadView>(BinaryPayloadView{owner, owner->data(), owner->size()});
    auto view = MessageView{};
    view.payload = star::base::TypedConstPtr::from(payload);
    return view;
}

}  // namespace

// monitor 的数据来源：worker 周期把本进程统计发布到普通通道，monitor 只是订阅者
TEST_CASE("StatsPublisher publishes worker stats on the ordinary stats channel") {
    const auto context = star::runtime::RuntimeFactory::create_native_context();
    REQUIRE(context != nullptr);

    // 先制造一条有写/读的通道，统计快照里才会出现它
    auto probe = ChannelDescriptor{};
    probe.name = "/test/stats/probe";
    probe.type_name = "test.probe";
    const auto probe_reader = context->message_bus().create_reader(probe, [](const MessageView&) {});
    const auto probe_writer = context->message_bus().create_writer(probe);
    const auto probe_result = probe_writer->write(make_binary_message({0x01, 0x02, 0x03}));
    REQUIRE(probe_result.ok());

    auto mutex = std::mutex{};
    auto received = std::make_shared<WorkerStats>();
    auto count = std::atomic<int>{0};
    const auto stats_reader = context->message_bus().create_reader(make_stats_descriptor(), [&](const MessageView& message) {
        try {
            auto stats = WorkerStats{};
            if (const auto typed = message.payload.as<WorkerStats>()) {
                // 进程内投递保持类型化指针（零拷贝），不经过字节解码
                stats = *typed;
            } else if (const auto binary = message.payload.as<BinaryPayloadView>()) {
                stats = WorkerStats::from_binary(binary->bytes());
            } else {
                return;
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                *received = std::move(stats);
            }
            count.fetch_add(1, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            std::printf("stats decode failed: %s\n", e.what());
        }
    });

    auto publisher = StatsPublisher{*context, "unit-test-group", 20};
    publisher.start();

    for (int i = 0; i < 100 && count.load(std::memory_order_relaxed) == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    publisher.stop();

    REQUIRE(count.load(std::memory_order_relaxed) > 0);
    REQUIRE(publisher.published_count() > 0);

    std::lock_guard<std::mutex> lock(mutex);
    REQUIRE_EQ(received->process_group, std::string("unit-test-group"));
    REQUIRE(received->pid > 0);
    REQUIRE(received->timestamp_ns > 0);

    auto found_probe = false;
    for (const auto& entry : received->channels) {
        if (entry.channel == "/test/stats/probe") {
            found_probe = true;
            REQUIRE(entry.write_count >= 1);
            REQUIRE(entry.deliver_count >= 1);
        }
    }
    REQUIRE(found_probe);
}

// stop() 必须立即唤醒等待中的发布线程，否则进程退出会白等一个发布周期
TEST_CASE("StatsPublisher stop does not wait for the next publish interval") {
    const auto context = star::runtime::RuntimeFactory::create_native_context();
    auto publisher = StatsPublisher{*context, "unit-test-stop", 10000};
    publisher.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    const auto begin = std::chrono::steady_clock::now();
    publisher.stop();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();
    REQUIRE(elapsed_ms < 1000);
}

#ifndef __STAR_RUNTIME_OBSERVABILITY_STATS_PUBLISHER_H__
#define __STAR_RUNTIME_OBSERVABILITY_STATS_PUBLISHER_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include "basement/core/logger/logger.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/schema/builtin/observability/worker_stats.s.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace star::runtime::observability {

// 统计通道名：worker 发布、monitor 订阅的约定。它是普通通道——
// 走路由/录制/类型校验，只是被登记为「系统发布者」以便跨进程可见。
inline constexpr const char* kStatsChannelName = "/starletto/stats";

// 周期把本进程的通道统计发布到 starletto.stats。
// 这样 monitor 不再自指：它是普通订阅者，读到的是其它进程真实 worker 的统计。
class StatsPublisher final {
  public:
    StatsPublisher(RuntimeContext& context, std::string process_group, int64_t interval_ms = 1000)
        : context_(&context), process_group_(std::move(process_group)), interval_ms_(interval_ms > 0 ? interval_ms : 1000) {}

    ~StatsPublisher() { stop(); }

    StatsPublisher(const StatsPublisher&) = delete;
    StatsPublisher& operator=(const StatsPublisher&) = delete;

    void start() {
        if (thread_.joinable()) {
            return;
        }
        if (process_group_.empty()) {
            process_group_ = "default";
        }
        // 路由模式（supervisor 下发 route plan）下，统计通道不在业务计划里，
        // 必须显式登记为系统发布者，否则会退化成 inproc、monitor 永远收不到
        if (auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context_->message_bus())) {
            routed->register_system_publisher(kStatsChannelName);
            shm_available_ = routed->shm_available();
        }
        auto descriptor = star::transport::ChannelDescriptor{};
        descriptor.name = kStatsChannelName;
        descriptor.type_name = "observability.worker_stats";
        descriptor.schema_type_id = star::generated::observability::worker_stats::WorkerStats::schema_type_id();
        writer_ = context_->message_bus().create_writer(descriptor);
        if (not writer_) {
            logW << "StatsPublisher: failed to create writer for " << kStatsChannelName;
            return;
        }
        running_.store(true, std::memory_order_relaxed);
        thread_ = std::thread([this]() { loop(); });
    }

    void stop() {
        running_.store(false, std::memory_order_relaxed);
        wait_cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
        writer_.reset();
    }

    auto published_count() const -> uint64_t { return published_.load(std::memory_order_relaxed); }

  private:
    void loop() {
        while (running_.load(std::memory_order_relaxed)) {
            publish_once();
            std::unique_lock<std::mutex> lock(wait_mutex_);
            wait_cv_.wait_for(lock, std::chrono::milliseconds(interval_ms_), [this]() { return not running_.load(std::memory_order_relaxed); });
        }
    }

    void publish_once() {
        auto message = star::generated::observability::worker_stats::WorkerStats{};
        message.process_group = process_group_;
        message.pid = static_cast<int64_t>(::getpid());
        message.timestamp_ns = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        message.transport = transport_name();
        message.shm_available = shm_available_;

        auto entries = std::vector<star::generated::observability::worker_stats::ChannelStatsEntry>{};
        for (const auto& snapshot : context_->message_bus().all_channel_stats()) {
            auto entry = star::generated::observability::worker_stats::ChannelStatsEntry{};
            entry.channel = snapshot.channel_name;
            entry.write_count = static_cast<int64_t>(snapshot.write_count);
            entry.deliver_count = static_cast<int64_t>(snapshot.deliver_count);
            entry.dropped_count = static_cast<int64_t>(snapshot.dropped_count);
            entry.callback_failures = static_cast<int64_t>(snapshot.callback_failures);
            entry.payload_bytes_total = static_cast<int64_t>(snapshot.payload_bytes_total);
            entry.avg_hz = snapshot.avg_hz;
            entry.latency_avg_us = static_cast<int64_t>(snapshot.latency_avg_us);
            entry.latency_p99_us = static_cast<int64_t>(snapshot.latency_p99_us);
            entry.latency_max_us = static_cast<int64_t>(snapshot.latency_max_us);
            entry.max_queue_depth = static_cast<int64_t>(snapshot.max_queue_depth);
            entries.push_back(std::move(entry));
        }
        message.channels = std::move(entries);

        auto msg = std::make_shared<const star::generated::observability::worker_stats::WorkerStats>(std::move(message));
        auto serialized = star::transport::SerializedMessage{};
        serialized.view.payload = star::base::TypedConstPtr::from(msg);
        serialized.view.schema_type_id = msg->schema_type_id();
        serialized.serializer.payload_size_hint = 0;
        serialized.serializer.encode_binary = [msg]() { return msg->encode_binary(); };

        const auto result = writer_->write_serialized(serialized);
        if (result.ok()) {
            published_.fetch_add(1, std::memory_order_relaxed);
        } else {
            // 统计发布失败不能影响业务，但要可见（每秒最多一条，避免刷屏）
            logW << "StatsPublisher: publish failed, status=" << star::transport::write_status_string(result.status)
                 << ", err=" << result.error_message;
        }
    }

    auto transport_name() const -> std::string {
        if (dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context_->message_bus()) != nullptr) {
            return shm_available_ ? "shm_routed" : "inproc";
        }
        return "native";
    }

    RuntimeContext* context_;
    std::string process_group_;
    int64_t interval_ms_;
    bool shm_available_{false};
    std::shared_ptr<star::transport::Writer> writer_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> published_{0};
    std::thread thread_;
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
};

}  // namespace star::runtime::observability

#endif  // __STAR_RUNTIME_OBSERVABILITY_STATS_PUBLISHER_H__

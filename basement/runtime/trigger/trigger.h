#ifndef __STAR_RUNTIME_TRIGGER_TRIGGER_H__
#define __STAR_RUNTIME_TRIGGER_TRIGGER_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <thread>
#include <unordered_set>

#include "basement/runtime/component/frame_inputs.h"
#include "basement/runtime/context/frame_context.h"

namespace star::runtime {

struct TriggerDescriptor {
    std::string type;
    std::string task_name;
};

class Trigger {
  public:
    virtual ~Trigger() = default;

    virtual void start() = 0;
    virtual void stop() = 0;
};

class SyncTrigger final : public Trigger {
  public:
    using FrameCallback = std::function<void(std::shared_ptr<const star::runtime::FrameInputs>)>;

    explicit SyncTrigger(FrameCallback callback) : callback_(std::move(callback)) {}

    void add_required_channel(const std::string& channel, const std::string& port_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        required_channels_[channel] = port_name;
    }

    void on_message(const std::string& channel, const star::transport::MessageView& message) {
        auto fired = std::shared_ptr<const star::runtime::FrameInputs>{};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto required = required_channels_.find(channel);
            if (required == required_channels_.end()) {
                return;
            }
            const auto snapshot = star::runtime::snapshot_trigger_message(message);
            if (not snapshot.has_value()) {
                return;  // shm 槽已被覆写：等下一帧，不能把撕裂数据当触发消息
            }
            pending_messages_[required->second] = *snapshot;
            arrived_.insert(channel);
            if (arrived_.size() < required_channels_.size()) {
                return;
            }
            auto inputs = std::make_shared<star::runtime::FrameInputs>();
            inputs->frame_id = ++frame_id_;
            inputs->timestamp_ns = star::runtime::FrameContext::now_monotonic_ns();
            inputs->messages = std::move(pending_messages_);
            pending_messages_.clear();
            arrived_.clear();
            fired = std::move(inputs);
        }
        // 回调必须在锁外派发：回调若重入 on_message()/frame_id() 会与本锁互斥而死锁
        if (callback_ && fired) {
            callback_(std::move(fired));
        }
    }

    void start() override {}

    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        arrived_.clear();
    }

    auto frame_id() const -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return frame_id_;
    }

    auto required_count() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return required_channels_.size();
    }

  private:
    FrameCallback callback_;
    std::unordered_map<std::string, std::string> required_channels_;
    std::unordered_set<std::string> arrived_;
    std::unordered_map<std::string, star::transport::MessageView> pending_messages_;
    int64_t frame_id_{0};
    mutable std::mutex mutex_;
};

class TimerTrigger final : public Trigger {
  public:
    using FrameCallback = std::function<void(std::shared_ptr<const star::runtime::FrameInputs>)>;

    TimerTrigger(FrameCallback callback, double cycle_ms) : callback_(std::move(callback)), cycle_ns_(static_cast<int64_t>(cycle_ms * 1e6)) {}

    ~TimerTrigger() override { stop(); }

    void start() override {
        if (running_.exchange(true)) {
            return;
        }
        thread_ = std::thread([this]() { run_loop(); });
    }

    void stop() override {
        if (not running_.exchange(false)) {
            return;
        }
        cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    auto frame_id() const -> int64_t { return frame_id_.load(); }

    auto cycle_ms() const -> double { return static_cast<double>(cycle_ns_) / 1e6; }

  private:
    void run_loop() {
        auto next_time = std::chrono::steady_clock::now();
        while (running_.load()) {
            const auto fid = frame_id_.fetch_add(1) + 1;
            if (callback_) {
                auto inputs = std::make_shared<star::runtime::FrameInputs>();
                inputs->frame_id = fid;
                inputs->timestamp_ns = star::runtime::FrameContext::now_monotonic_ns();
                callback_(std::move(inputs));
            }
            next_time += std::chrono::nanoseconds(cycle_ns_);
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_until(lock, next_time, [this]() { return not running_.load(); });
        }
    }

    FrameCallback callback_;
    int64_t cycle_ns_;
    std::atomic<int64_t> frame_id_{0};
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_TRIGGER_TRIGGER_H__

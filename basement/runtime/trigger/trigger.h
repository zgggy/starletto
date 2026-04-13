#ifndef __STAR_RUNTIME_TRIGGER_TRIGGER_H__
#define __STAR_RUNTIME_TRIGGER_TRIGGER_H__

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

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
    using FrameCallback = std::function<void(int64_t frame_id)>;

    explicit SyncTrigger(FrameCallback callback) : callback_(std::move(callback)) {}

    void add_required_channel(const std::string& channel) {
        std::lock_guard<std::mutex> lock(mutex_);
        required_channels_.insert(channel);
    }

    void on_message(const std::string& channel) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (required_channels_.find(channel) == required_channels_.end()) {
            return;
        }
        arrived_.insert(channel);
        if (arrived_.size() >= required_channels_.size()) {
            ++frame_id_;
            const auto fid = frame_id_;
            arrived_.clear();
            if (callback_) {
                callback_(fid);
            }
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
    std::unordered_set<std::string> required_channels_;
    std::unordered_set<std::string> arrived_;
    int64_t frame_id_{0};
    mutable std::mutex mutex_;
};

class TimerTrigger final : public Trigger {
  public:
    using FrameCallback = std::function<void(int64_t frame_id)>;

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
                callback_(fid);
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

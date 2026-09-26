#ifndef __STAR_RUNTIME_SCHEDULER_NATIVE_SCHEDULER_H__
#define __STAR_RUNTIME_SCHEDULER_NATIVE_SCHEDULER_H__

#include <pthread.h>
#include <sched.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"
#include "basement/runtime/context/frame_context.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/runtime/scheduler/scheduler.h"

namespace star::runtime {

namespace {

auto parse_affinity_string(const std::string& s) -> std::vector<int> {
    auto result = std::vector<int>{};
    if (s.empty()) {
        return result;
    }
    auto tokens = std::vector<std::string>{};
    auto cur = std::string{};
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == ',' || s[i] == ' ') {
            if (!cur.empty()) {
                tokens.emplace_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(s[i]);
        }
    }
    if (!cur.empty()) {
        tokens.emplace_back(cur);
    }
    for (const auto& token : tokens) {
        if (token.find('-') != std::string::npos) {
            auto parts = std::vector<std::string>{};
            auto part = std::string{};
            for (size_t i = 0; i < token.size(); ++i) {
                if (token[i] == '-') {
                    if (!part.empty()) {
                        parts.emplace_back(part);
                        part.clear();
                    }
                } else {
                    part.push_back(token[i]);
                }
            }
            if (!part.empty()) {
                parts.emplace_back(part);
            }
            if (parts.size() == 2) {
                try {
                    auto start = std::stoi(parts[0]);
                    auto end = std::stoi(parts[1]);
                    for (int i = start; i <= end; ++i) {
                        result.emplace_back(i);
                    }
                } catch (...) {
                }
            }
        } else {
            try {
                result.emplace_back(std::stoi(token));
            } catch (...) {
            }
        }
    }
    return result;
}

void apply_affinity_to_thread(std::thread& t, const std::vector<int>& cpu_affinity_ids, const std::optional<std::string>& cpu_affinity_str) {
    if (cpu_affinity_ids.empty()) {
        return;
    }
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (const auto cpu_id : cpu_affinity_ids) {
        CPU_SET(cpu_id, &cpuset);
    }
    const auto rc = pthread_setaffinity_np(t.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        logW << "NativeScheduler apply_affinity failed: rc=" << rc << ", cpu_affinity=" << (cpu_affinity_str.has_value() ? *cpu_affinity_str : "");
    }
#else
    // 非 Linux 平台无对等的 CPU 亲和性 API；此处仅影响调度性能，不影响正确性
    (void)t;
    logW << "NativeScheduler cpu_affinity ignored on this platform: " << (cpu_affinity_str.has_value() ? *cpu_affinity_str : "");
#endif
}

}  // namespace

class NativeScheduler final : public Scheduler {
  public:
    NativeScheduler() = default;
    ~NativeScheduler() override {
        try {
            stop();
        } catch (...) {
        }
    }

    void set_context(RuntimeContext* ctx) override { context_ = ctx; }

    void configure(const SchedulerConfig& config) override {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }

    void apply_plan(const ProcessGroupPlan& plan) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (plan.scheduler_type.has_value()) {
            scheduler_type_ = *plan.scheduler_type;
        }
        if (plan.choreography.has_value()) {
            if (plan.choreography->pool_threads.has_value()) {
                config_.worker_threads = *plan.choreography->pool_threads;
            }
            if (plan.choreography->cpu_affinity.has_value() && !plan.choreography->cpu_affinity->empty()) {
                cpu_affinity_str_ = *plan.choreography->cpu_affinity;
                cpu_affinity_ids_ = parse_affinity_string(*cpu_affinity_str_);
                if (cpu_affinity_ids_.empty()) {
                    logW << "NativeScheduler apply_plan: failed to parse cpu_affinity: " << *cpu_affinity_str_;
                }
            }
            if (plan.choreography->tasks.has_value()) {
                for (const auto& task_plan : *plan.choreography->tasks) {
                    task_priorities_[task_plan.name] = task_plan.priority.value_or(0);
                }
            }
        }
        logI << "NativeScheduler applied plan: group=" << plan.name << ", type=" << scheduler_type_ << ", threads=" << config_.worker_threads
             << ", cpu_affinity=" << (cpu_affinity_str_.has_value() ? *cpu_affinity_str_ : "<none>");
    }

    void register_task(const TaskDescriptor& descriptor) override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("NativeScheduler register_task failed, empty task name");
        }
        if (not descriptor.callback) {
            throw star::excep::RuntimeError("NativeScheduler register_task failed, empty callback: ", descriptor.name);
        }
        std::lock_guard<std::mutex> lock(mutex_);
        TaskState state;
        state.desc = descriptor;
        state.next_frame_id = 1;
        state.stats.task_name = descriptor.name;
        tasks_[descriptor.name] = std::move(state);
        cv_timer_.notify_all();
    }

    void notify(const std::string& task_name, int64_t frame_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not running_) {
            pending_notifications_.emplace(task_name, frame_id);
            return;
        }
        push_ready_unlocked(task_name, frame_id);
        cv_ready_.notify_one();
    }

    void start() override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) {
            return;
        }
        running_ = true;
        stop_requested_ = false;
        start_time_ = std::chrono::steady_clock::now();
        last_stats_dump_ = start_time_;
        for (auto& [_, t] : tasks_) {
            t.next_due = start_time_;
        }

        const auto thread_count = config_.worker_threads > 0 ? config_.worker_threads : 1;
        workers_.reserve(static_cast<size_t>(thread_count));
        for (int64_t i = 0; i < thread_count; ++i) {
            workers_.emplace_back([this]() { worker_loop(); });
            apply_affinity_to_thread(workers_.back(), cpu_affinity_ids_, cpu_affinity_str_);
        }
        timer_thread_ = std::thread([this]() { timer_loop(); });
        while (not pending_notifications_.empty()) {
            auto [name, frame_id] = pending_notifications_.front();
            pending_notifications_.pop();
            push_ready_unlocked(name, frame_id);
        }
        cv_ready_.notify_all();
        cv_timer_.notify_all();
        logI << "[SchedulerStats] initial state:";
        dump_stats_unlocked();
    }

    auto total_invocation_count(const std::string& task_name) const -> uint64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = tasks_.find(task_name);
        return it == tasks_.end() ? 0 : it->second.stats.total_invocations;
    }

    auto failed_invocation_count(const std::string& task_name) const -> uint64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = tasks_.find(task_name);
        return it == tasks_.end() ? 0 : it->second.stats.failed_invocations;
    }

    void stop() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (not running_) {
                return;
            }
            stop_requested_ = true;
            running_ = false;
        }
        cv_ready_.notify_all();
        cv_timer_.notify_all();

        if (timer_thread_.joinable()) {
            timer_thread_.join();
        }
        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
        workers_.clear();

        std::lock_guard<std::mutex> lock(mutex_);
        while (not ready_.empty()) {
            ready_.pop();
        }
        while (not pending_notifications_.empty()) {
            pending_notifications_.pop();
        }
    }

  private:
    struct ReadyItem {
        std::string task_name;
        FrameContext frame;
        int64_t priority{0};
        uint64_t seq{0};
    };

    struct ReadyFirst {
        auto operator()(const ReadyItem& a, const ReadyItem& b) const -> bool {
            if (a.priority != b.priority) {
                return a.priority < b.priority;
            }
            return a.seq > b.seq;
        }
    };

    struct TaskStats {
        std::string task_name;
        uint64_t total_invocations{0};
        uint64_t failed_invocations{0};
        double total_wall_time_us{0.0};
        double last_wall_time_us{0.0};
        double avg_wall_time_us() const { return total_invocations > 0 ? total_wall_time_us / static_cast<double>(total_invocations) : 0.0; }
    };

    struct TaskState {
        TaskDescriptor desc{};
        std::chrono::steady_clock::time_point next_due{};
        int64_t next_frame_id{1};
        TaskStats stats{};
    };

    void push_ready_unlocked(const std::string& task_name, int64_t frame_id) {
        const auto it = tasks_.find(task_name);
        int64_t priority = 0;
        if (it != tasks_.end()) {
            priority = it->second.desc.priority;
        }
        if (auto pit = task_priorities_.find(task_name); pit != task_priorities_.end()) {
            priority = pit->second;
        }
        FrameContext ctx;
        ctx.frame_id = frame_id;
        ctx.monotonic_time_ns = FrameContext::now_monotonic_ns();
        ready_.push(ReadyItem{task_name, ctx, priority, ready_seq_++});
    }

    void worker_loop() {
        for (;;) {
            ReadyItem item;
            TaskDescriptor desc;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_ready_.wait(lock, [this]() { return stop_requested_ || !ready_.empty(); });
                if (stop_requested_) {
                    return;
                }
                item = std::move(ready_.top());
                ready_.pop();
                const auto it = tasks_.find(item.task_name);
                if (it == tasks_.end()) {
                    continue;
                }
                desc = it->second.desc;
            }
            const auto t0 = std::chrono::steady_clock::now();
            if (context_) {
                context_->set_current_frame(item.frame);
            }
            auto failed = false;
            try {
                desc.callback(item.frame.frame_id);
            } catch (const std::exception& e) {
                failed = true;
                logE << "NativeScheduler task failed: " << desc.name << ", err: " << e.what();
            } catch (...) {
                failed = true;
                logE << "NativeScheduler task failed: " << desc.name << ", err: unknown exception";
            }
            const auto t1 = std::chrono::steady_clock::now();
            const auto elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto& stats = tasks_[item.task_name].stats;
                stats.total_invocations += 1;
                if (failed) {
                    // 逃逸到调度层的异常计入统计，使故障可观测而非只留在日志里
                    stats.failed_invocations += 1;
                }
                stats.total_wall_time_us += elapsed_us;
                stats.last_wall_time_us += elapsed_us;
            }
        }
    }

    void timer_loop() {
        for (;;) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_requested_) {
                return;
            }

            auto next_wakeup = std::chrono::steady_clock::time_point::max();
            const auto now = std::chrono::steady_clock::now();
            for (auto& [name, task] : tasks_) {
                if (task.desc.period_ms <= 0) {
                    continue;
                }
                if (task.next_due <= now) {
                    const auto frame_id = task.next_frame_id++;
                    push_ready_unlocked(name, frame_id);
                    task.next_due = now + std::chrono::milliseconds(task.desc.period_ms);
                    cv_ready_.notify_one();
                }
                if (task.next_due < next_wakeup) {
                    next_wakeup = task.next_due;
                }
            }

            if (next_wakeup == std::chrono::steady_clock::time_point::max()) {
                cv_timer_.wait(lock, [this]() { return stop_requested_; });
            } else {
                cv_timer_.wait_until(lock, next_wakeup, [this]() { return stop_requested_; });
            }
            if (not stop_requested_) {
                const auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_dump_).count() >= 60) {
                    logI << "[SchedulerStats] periodic dump:";
                    dump_stats();
                    last_stats_dump_ = now;
                }
            }
        }
    }

    void dump_stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        dump_stats_unlocked();
    }

    void dump_stats_unlocked() {
        for (const auto& [name, task] : tasks_) {
            logI << "[SchedulerStats] task=" << task.stats.task_name << ", invocations=" << task.stats.total_invocations << ", failed=" << task.stats.failed_invocations
                 << ", avg_time_us=" << task.stats.avg_wall_time_us() << ", last_time_us=" << task.stats.last_wall_time_us;
        }
    }

    mutable std::mutex mutex_;
    SchedulerConfig config_{};
    bool running_{false};
    bool stop_requested_{false};
    std::chrono::steady_clock::time_point start_time_{};
    std::unordered_map<std::string, TaskState> tasks_;
    std::priority_queue<ReadyItem, std::vector<ReadyItem>, ReadyFirst> ready_;
    uint64_t ready_seq_{0};
    std::string scheduler_type_{"default"};
    std::unordered_map<std::string, int64_t> task_priorities_;
    std::optional<std::string> cpu_affinity_str_;
    std::vector<int> cpu_affinity_ids_;
    std::queue<std::pair<std::string, int64_t>> pending_notifications_;
    std::condition_variable cv_ready_;
    std::condition_variable cv_timer_;
    std::thread timer_thread_;
    std::vector<std::thread> workers_;
    std::chrono::steady_clock::time_point last_stats_dump_{};
    RuntimeContext* context_{nullptr};
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_SCHEDULER_NATIVE_SCHEDULER_H__

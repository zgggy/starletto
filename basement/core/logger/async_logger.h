#ifndef __BASEMENT_ASYNC_LOGGER_H__
#define __BASEMENT_ASYNC_LOGGER_H__

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"

namespace star::base {

// 异步日志装饰器：调用方只做格式化 + 入队，文件 I/O / flush 在写线程上。
//
// 有界队列：队列满时丢弃新日志并计数——宁可丢日志，也不让磁盘抖动反压业务线程。
// flush() 会等待队列清空且写线程空闲，然后 flush 内层 logger；析构时同样先排空。
class AsyncLogger final : public Logger {
  public:
    explicit AsyncLogger(std::shared_ptr<Logger> inner, size_t max_queue = 16384)
        : inner_(std::move(inner)), max_queue_(max_queue > 0 ? max_queue : 1) {
        if (not inner_) {
            throw star::excep::RuntimeError("AsyncLogger requires a non-null inner logger");
        }
        level_ = inner_->get_level();
        // 格式化发生在装饰器这一层（内层只收到已格式化的行），格式必须从这里继承
        format_ = inner_->get_format();
        thread_ = std::thread([this]() { run(); });
    }

    ~AsyncLogger() override { stop(); }

    AsyncLogger(const AsyncLogger&) = delete;
    AsyncLogger& operator=(const AsyncLogger&) = delete;

    void set_level(Level level) override {
        level_ = level;
        inner_->set_level(level);
    }

    auto get_level() const -> Level override { return inner_->get_level(); }

    void set_format(LogFormat format) override { inner_->set_format(format); }

    void flush() override {
        std::unique_lock<std::mutex> lock(mutex_);
        drain_cv_.wait(lock, [this]() { return pending_ == 0; });
        lock.unlock();
        inner_->flush();
    }

    auto dropped() const -> uint64_t { return dropped_.load(std::memory_order_relaxed); }

  protected:
    void write(Level level, const std::string& formatted) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.size() >= max_queue_) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            queue_.push_back(Entry{level, formatted});
            ++pending_;
        }
        cv_.notify_one();
    }

  private:
    struct Entry {
        Level level{Level::INFO};
        std::string line;
    };

    void stop() {
        if (stopped_.exchange(true, std::memory_order_relaxed)) {
            return;
        }
        cv_.notify_all();
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    void run() {
        for (;;) {
            auto entry = Entry{};
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this]() { return stopped_.load(std::memory_order_relaxed) or not queue_.empty(); });
                if (queue_.empty()) {
                    if (stopped_.load(std::memory_order_relaxed)) {
                        break;
                    }
                    continue;
                }
                entry = std::move(queue_.front());
                queue_.pop_front();
            }
            inner_->write_line(entry.level, std::move(entry.line));
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (pending_ > 0) {
                    --pending_;
                }
                if (pending_ == 0) {
                    drain_cv_.notify_all();
                }
            }
        }

        // 退出前把队列里剩下的写完，再报告丢弃数量（否则退出前的日志全丢）
        for (;;) {
            auto entry = Entry{};
            {
                std::lock_guard<std::mutex> lock(mutex_);
                if (queue_.empty()) {
                    break;
                }
                entry = std::move(queue_.front());
                queue_.pop_front();
                if (pending_ > 0) {
                    --pending_;
                }
            }
            inner_->write_line(entry.level, std::move(entry.line));
        }
        const auto dropped = dropped_.load(std::memory_order_relaxed);
        if (dropped > 0) {
            inner_->write_line(Level::WARN, "AsyncLogger dropped " + std::to_string(dropped) + " log lines (queue full)");
        }
        inner_->flush();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_ = 0;
            drain_cv_.notify_all();
        }
    }

    std::shared_ptr<Logger> inner_;
    const size_t max_queue_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable drain_cv_;
    std::deque<Entry> queue_;
    size_t pending_{0};
    std::atomic<uint64_t> dropped_{0};
    std::atomic<bool> stopped_{false};
    std::thread thread_;
};

}  // namespace star::base

#endif  // __BASEMENT_ASYNC_LOGGER_H__

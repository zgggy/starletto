#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "basement/core/logger/async_logger.h"
#include "basement/core/logger/file_logger.h"
#include "basement/core/logger/logger.h"

#include "tests/support/star_test.h"

namespace {

using star::base::AsyncLogger;
using star::base::Level;
using star::base::LogFormat;
using star::base::Logger;
using star::base::LoggerManager;
using star::base::ModuleLogScope;

class CapturingLogger final : public Logger {
  public:
    using Logger::Logger;

    auto lines() const -> std::vector<std::pair<Level, std::string>> {
        std::lock_guard<std::mutex> lock(mutex_);
        return lines_;
    }

    void flush() override { flushed.store(true); }
    auto was_flushed() const -> bool { return flushed.load(); }

  protected:
    void write(Level level, const std::string& formatted) override {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_.emplace_back(level, formatted);
    }

  private:
    mutable std::mutex mutex_;
    std::vector<std::pair<Level, std::string>> lines_;
    std::atomic<bool> flushed{false};
};

class GatedLogger final : public Logger {
  public:
    void release() { released.store(true); }

  protected:
    void write(Level /*level*/, const std::string& /*formatted*/) override {
        while (not released.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

  private:
    std::atomic<bool> released{false};
};

auto contains(const std::string& haystack, const std::string& needle) -> bool {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

// 多进程排查的第一需求：日志能直接看出是哪条日志、哪个进程、哪个线程
TEST_CASE("text log records carry pid and thread id") {
    auto logger = CapturingLogger{};
    logger.log(Level::INFO, "hello", "some/dir/app.cc", 42, "run");

    const auto lines = logger.lines();
    REQUIRE_EQ(lines.size(), 1U);
    REQUIRE_EQ(lines[0].first, Level::INFO);
    REQUIRE(contains(lines[0].second, "INFO"));
    REQUIRE(contains(lines[0].second, "pid="));
    REQUIRE(contains(lines[0].second, "tid="));
    REQUIRE(contains(lines[0].second, "app.cc:42"));
    REQUIRE(contains(lines[0].second, "run hello"));
}

TEST_CASE("json log records are single-line and escape the message") {
    auto logger = CapturingLogger{LogFormat::JSON};
    logger.log(Level::WARN, "quote=\" and\nnewline", "app.cc", 7, "f");

    const auto lines = logger.lines();
    REQUIRE_EQ(lines.size(), 1U);
    const auto& line = lines[0].second;
    REQUIRE(line.front() == '{');
    REQUIRE(line.back() == '}');
    REQUIRE(contains(line, "\"level\":\"WARN\""));
    REQUIRE(contains(line, "\"pid\":"));
    REQUIRE(contains(line, "\"tid\":"));
    REQUIRE(contains(line, "\"file\":\"app.cc\""));
    REQUIRE(contains(line, "\"line\":7"));
    REQUIRE(contains(line, "quote=\\\" and\\nnewline"));
    // JSON 必须保持单行：消息里的换行被转义，不能真的输出换行
    REQUIRE(not contains(line, "\n"));
}

TEST_CASE("async logger drains every queued line on flush") {
    const auto inner = std::make_shared<CapturingLogger>();
    auto logger = AsyncLogger{inner, 1024};
    for (int i = 0; i < 200; ++i) {
        logger.log(Level::INFO, "line " + std::to_string(i), __FILE__, __LINE__, __FUNCTION__);
    }
    logger.flush();

    REQUIRE_EQ(logger.dropped(), uint64_t{0});
    REQUIRE_EQ(inner->lines().size(), 200U);
    REQUIRE(inner->was_flushed());
}

TEST_CASE("async logger drops instead of blocking when the queue is full") {
    const auto inner = std::make_shared<GatedLogger>();
    auto logger = AsyncLogger{inner, 1};
    for (int i = 0; i < 50; ++i) {
        logger.log(Level::INFO, "line", __FILE__, __LINE__, __FUNCTION__);
    }
    // 写线程被 gate 卡住：队列满后必须丢弃并计数，而不是阻塞调用方
    REQUIRE(logger.dropped() > 0);
    inner->release();
    logger.flush();
}

// per-module 级别：module.conf 的 logging.level 只影响该模块的日志
TEST_CASE("module log scope overrides the global level") {
    auto capturing = std::make_shared<CapturingLogger>();
    capturing->set_level(Level::WARN);
    LoggerManager::get().set_logger(capturing);
    LoggerManager::get().set_module_level("planning", Level::DEBUG);

    {
        ModuleLogScope scope{"planning"};
        logD << "debug from planning";
        logW << "warn from planning";
    }
    {
        ModuleLogScope scope{"control"};
        logD << "debug from control";
        logW << "warn from control";
    }

    const auto lines = capturing->lines();
    auto planning_debug = false;
    auto control_debug = false;
    auto planning_warn = false;
    auto control_warn = false;
    for (const auto& [_, line] : lines) {
        planning_debug = planning_debug or contains(line, "debug from planning");
        control_debug = control_debug or contains(line, "debug from control");
        planning_warn = planning_warn or contains(line, "warn from planning");
        control_warn = control_warn or contains(line, "warn from control");
    }
    REQUIRE(planning_debug);
    REQUIRE(not control_debug);
    REQUIRE(planning_warn);
    REQUIRE(control_warn);

    LoggerManager::get().clear_module_levels();
    LoggerManager::get().set_logger(std::make_shared<CapturingLogger>());
}

TEST_CASE("log level strings parse case-insensitively") {
    auto level = Level::INFO;
    REQUIRE(star::base::parse_level("debug", &level));
    REQUIRE_EQ(level, Level::DEBUG);
    REQUIRE(star::base::parse_level("WaRn", &level));
    REQUIRE_EQ(level, Level::WARN);
    REQUIRE(star::base::parse_level("ERROR", &level));
    REQUIRE_EQ(level, Level::ERROR);
    REQUIRE(not star::base::parse_level("verbose", &level));
}

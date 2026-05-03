#ifndef __BASEMENT_LOGGER_H__
#define __BASEMENT_LOGGER_H__

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

namespace star::base {

enum class Level { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
  public:
    virtual ~Logger() = default;

    void log(Level level, const std::string& message, const char* file, int line, const char* function) {
        if (level < level_) {
            return;
        }
        const auto formatted = format_message(level, message, file, line, function);
        write(level, formatted);
    }

    virtual void set_level(Level level) { level_ = level; }
    virtual Level get_level() const { return level_; }

    static auto is_enabled(Level level) -> bool;

  protected:
    Level level_ = Level::INFO;

    virtual void write(Level level, const std::string& formatted) = 0;

    static auto short_file(const char* file) -> const char* {
        if (file == nullptr) {
            return "";
        }
        const auto* last = std::strrchr(file, '/');
        return last ? last + 1 : file;
    }

    static std::string format_message(Level level, const std::string& message, const char* file, int line, const char* function) {
        auto oss = std::ostringstream{};
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &tt);
#else
        localtime_r(&tt, &tm_buf);
#endif
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(6) << us.count();
        oss << ' ' << std::left << std::setw(5) << level_string(level);
        oss << " [" << std::right << std::setw(5) << std::this_thread::get_id() << ']';
        const auto f = short_file(file);
        oss << ' ' << f << ':' << line;
        if (function != nullptr && std::strlen(function) > 0) {
            oss << ' ' << function;
        }
        oss << ' ' << message;
        return oss.str();
    }

    static const char* level_string(Level level) {
        switch (level) {
            case Level::DEBUG:
                return "DEBUG";
            case Level::INFO:
                return "INFO";
            case Level::WARN:
                return "WARN";
            case Level::ERROR:
                return "ERROR";
            default:
                return "UNKNOWN";
        }
    }
};

class StdoutLogger : public Logger {
  protected:
    void write(Level /*level*/, const std::string& formatted) override {
        static auto mutex = std::mutex{};
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << formatted << "\n";
    }
};

class LogStream {
  public:
    LogStream(std::shared_ptr<Logger> logger, Level level, const char* file, int line, const char* function)
        : logger_(std::move(logger)), level_(level), file_(file), line_(line), function_(function) {}

    ~LogStream() {
        if (logger_) {
            logger_->log(level_, oss_.str(), file_, line_, function_);
        }
    }

    template <typename T>
    LogStream& operator<<(T&& value) {
        oss_ << std::forward<T>(value);
        return *this;
    }

  private:
    std::shared_ptr<Logger> logger_;
    Level level_;
    const char* file_;
    int line_;
    const char* function_;
    std::ostringstream oss_;
};

class LoggerManager {
  public:
    static auto get() -> LoggerManager& {
        static LoggerManager instance;
        return instance;
    }

    void set_logger(std::unique_ptr<Logger> logger) {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_ = std::shared_ptr<Logger>(std::move(logger));
    }

    void set_logger(std::shared_ptr<Logger> logger) {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_ = std::move(logger);
    }

    auto get_logger() -> Logger* { return get_logger_shared().get(); }

    auto get_logger_shared() -> std::shared_ptr<Logger> {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not logger_) {
            logger_ = std::make_shared<StdoutLogger>();
        }
        return logger_;
    }

  private:
    LoggerManager() = default;
    ~LoggerManager() = default;
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    std::mutex mutex_;
    std::shared_ptr<Logger> logger_;
};

inline auto Logger::is_enabled(Level level) -> bool { return level >= LoggerManager::get().get_logger()->get_level(); }

}  // namespace star::base

#define _STAR_LOG_STREAM(level) \
    for (bool _star_log_ok = star::base::Logger::is_enabled(level); _star_log_ok; _star_log_ok = false) \
    star::base::LogStream(star::base::LoggerManager::get().get_logger_shared(), level, __FILE__, __LINE__, __FUNCTION__)

#define logD _STAR_LOG_STREAM(star::base::Level::DEBUG)
#define logI _STAR_LOG_STREAM(star::base::Level::INFO)
#define logW _STAR_LOG_STREAM(star::base::Level::WARN)
#define logE _STAR_LOG_STREAM(star::base::Level::ERROR)

#endif  // __BASEMENT_LOGGER_H__

#ifndef __BASEMENT_LOGGER_H__
#define __BASEMENT_LOGGER_H__

#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

#if defined(_WIN32)
#else
#include <unistd.h>
#endif

namespace star::base {

enum class Level { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

enum class LogFormat {
    TEXT = 0,
    JSON = 1,
};

struct LogRecord {
    int64_t timestamp_us{0};
    Level level{Level::INFO};
    int32_t pid{0};
    std::string tid;
    const char* module{nullptr};
    const char* file{nullptr};
    int line{0};
    const char* function{nullptr};
    std::string message;
};

// 当前线程正在执行的 module（由调度器在调用 process_once 前设置）。
// 用来做 per-module 日志级别：查表只在进入任务时做一次，不进入热路径。
struct ModuleLogState {
    const char* module{nullptr};
    std::optional<Level> level{};
};

inline thread_local ModuleLogState tls_log_state{};

class Logger;

class ModuleLogScope {
  public:
    explicit ModuleLogScope(const char* module);
    ~ModuleLogScope() { tls_log_state = previous_; }

    ModuleLogScope(const ModuleLogScope&) = delete;
    ModuleLogScope& operator=(const ModuleLogScope&) = delete;

  private:
    ModuleLogState previous_{};
};

inline auto parse_level(std::string_view text, Level* out) -> bool {
    if (out == nullptr or text.empty()) {
        return false;
    }
    auto lower = std::string{};
    lower.reserve(text.size());
    for (const char ch : text) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    if (lower == "debug") {
        *out = Level::DEBUG;
        return true;
    }
    if (lower == "info") {
        *out = Level::INFO;
        return true;
    }
    if (lower == "warn" or lower == "warning") {
        *out = Level::WARN;
        return true;
    }
    if (lower == "error") {
        *out = Level::ERROR;
        return true;
    }
    return false;
}

inline auto level_name(Level level) -> const char* {
    switch (level) {
        case Level::DEBUG:
            return "DEBUG";
        case Level::INFO:
            return "INFO";
        case Level::WARN:
            return "WARN";
        case Level::ERROR:
            return "ERROR";
    }
    return "UNKNOWN";
}

class Logger {
  public:
    explicit Logger(LogFormat format = LogFormat::TEXT) : format_(format) {}
    virtual ~Logger() = default;

    void log(Level level, const std::string& message, const char* file, int line, const char* function) {
        if (not should_log(level)) {
            return;
        }
        auto record = LogRecord{};
        record.timestamp_us = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
        record.level = level;
#if defined(_WIN32)
        record.pid = 0;
#else
        record.pid = static_cast<int32_t>(::getpid());
#endif
        record.tid = current_tid();
        record.module = tls_log_state.module;
        record.file = file;
        record.line = line;
        record.function = function;
        record.message = message;
        write(level, format_record(record));
    }

    // 供 AsyncLogger 之类的装饰器在同进程内转发已格式化的行
    void write_line(Level level, std::string line) { write(level, line); }

    virtual void set_level(Level level) { level_ = level; }
    virtual auto get_level() const -> Level { return level_; }
    virtual void set_format(LogFormat format) { format_ = format; }
    virtual auto get_format() const -> LogFormat { return format_; }
    virtual void flush() {}

    static auto is_enabled(Level level) -> bool;

  protected:
    Level level_ = Level::INFO;
    LogFormat format_{LogFormat::TEXT};

    virtual void write(Level level, const std::string& formatted) = 0;

    auto should_log(Level level) const -> bool {
        if (tls_log_state.level.has_value()) {
            return level >= *tls_log_state.level;
        }
        return level >= level_;
    }

    static auto short_file(const char* file) -> const char* {
        if (file == nullptr) {
            return "";
        }
        const auto* last = std::strrchr(file, '/');
        return last ? last + 1 : file;
    }

    static auto current_tid() -> std::string {
        auto oss = std::ostringstream{};
        oss << std::this_thread::get_id();
        return oss.str();
    }

    static auto timestamp_string(int64_t timestamp_us) -> std::string {
        const auto seconds = static_cast<std::time_t>(timestamp_us / 1000000);
        const auto micros = timestamp_us % 1000000;
        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &seconds);
#else
        localtime_r(&seconds, &tm_buf);
#endif
        auto oss = std::ostringstream{};
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
        oss << '.' << std::setfill('0') << std::setw(6) << micros;
        return oss.str();
    }

    static void append_json_string(std::ostringstream& oss, const std::string& text) {
        oss << '"';
        for (const char ch : text) {
            switch (ch) {
                case '"':
                    oss << "\\\"";
                    break;
                case '\\':
                    oss << "\\\\";
                    break;
                case '\n':
                    oss << "\\n";
                    break;
                case '\r':
                    oss << "\\r";
                    break;
                case '\t':
                    oss << "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20) {
                        oss << "\\u" << std::hex << std::setfill('0') << std::setw(4)
                            << static_cast<int>(static_cast<unsigned char>(ch)) << std::dec << std::setfill(' ');
                    } else {
                        oss << ch;
                    }
                    break;
            }
        }
        oss << '"';
    }

    auto format_record(const LogRecord& record) const -> std::string {
        if (format_ == LogFormat::JSON) {
            return format_json(record);
        }
        return format_text(record);
    }

    static auto format_text(const LogRecord& record) -> std::string {
        auto oss = std::ostringstream{};
        oss << timestamp_string(record.timestamp_us);
        oss << ' ' << std::left << std::setw(5) << level_name(record.level);
        oss << " [pid=" << record.pid << " tid=" << record.tid << ']';
        const auto f = short_file(record.file);
        oss << ' ' << f << ':' << record.line;
        if (record.function != nullptr && std::strlen(record.function) > 0) {
            oss << ' ' << record.function;
        }
        oss << ' ' << record.message;
        return oss.str();
    }

    static auto format_json(const LogRecord& record) -> std::string {
        auto oss = std::ostringstream{};
        oss << "{\"ts\":";
        append_json_string(oss, timestamp_string(record.timestamp_us));
        oss << ",\"level\":";
        append_json_string(oss, level_name(record.level));
        oss << ",\"pid\":" << record.pid;
        oss << ",\"tid\":";
        append_json_string(oss, record.tid);
        if (record.module != nullptr && std::strlen(record.module) > 0) {
            oss << ",\"module\":";
            append_json_string(oss, record.module);
        }
        oss << ",\"file\":";
        append_json_string(oss, short_file(record.file));
        oss << ",\"line\":" << record.line;
        if (record.function != nullptr && std::strlen(record.function) > 0) {
            oss << ",\"func\":";
            append_json_string(oss, record.function);
        }
        oss << ",\"msg\":";
        append_json_string(oss, record.message);
        oss << '}';
        return oss.str();
    }
};

class StdoutLogger : public Logger {
  public:
    explicit StdoutLogger(LogFormat format = LogFormat::TEXT) : Logger(format) {}

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

    // per-module 级别覆盖：module.conf 的 logging.level 经 plan 带到这里
    void set_module_level(const std::string& module, Level level) {
        if (module.empty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(level_mutex_);
        module_levels_[module] = level;
    }

    auto module_level(const std::string& module) const -> std::optional<Level> {
        std::lock_guard<std::mutex> lock(level_mutex_);
        const auto it = module_levels_.find(module);
        if (it == module_levels_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void clear_module_levels() {
        std::lock_guard<std::mutex> lock(level_mutex_);
        module_levels_.clear();
    }

    void flush() {
        auto logger = get_logger_shared();
        if (logger) {
            logger->flush();
        }
    }

  private:
    LoggerManager() = default;
    ~LoggerManager() = default;
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    std::mutex mutex_;
    std::shared_ptr<Logger> logger_;

    mutable std::mutex level_mutex_;
    std::unordered_map<std::string, Level> module_levels_;
};

inline ModuleLogScope::ModuleLogScope(const char* module) : previous_(tls_log_state) {
    if (module == nullptr || module[0] == '\0') {
        tls_log_state = ModuleLogState{};
        return;
    }
    tls_log_state.module = module;
    tls_log_state.level = LoggerManager::get().module_level(module);
}

inline auto Logger::is_enabled(Level level) -> bool {
    if (tls_log_state.level.has_value()) {
        return level >= *tls_log_state.level;
    }
    return level >= LoggerManager::get().get_logger()->get_level();
}

}  // namespace star::base

#define _STAR_LOG_STREAM(level) \
    for (bool _star_log_ok = star::base::Logger::is_enabled(level); _star_log_ok; _star_log_ok = false) \
    star::base::LogStream(star::base::LoggerManager::get().get_logger_shared(), level, __FILE__, __LINE__, __FUNCTION__)

#define logD _STAR_LOG_STREAM(star::base::Level::DEBUG)
#define logI _STAR_LOG_STREAM(star::base::Level::INFO)
#define logW _STAR_LOG_STREAM(star::base::Level::WARN)
#define logE _STAR_LOG_STREAM(star::base::Level::ERROR)

#endif  // __BASEMENT_LOGGER_H__

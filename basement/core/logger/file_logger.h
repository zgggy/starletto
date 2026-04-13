#ifndef __BASEMENT_FILE_LOGGER_H__
#define __BASEMENT_FILE_LOGGER_H__

#include "basement/core/logger/logger.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace star::base {

class FileLogger final : public Logger {
public:
    explicit FileLogger(std::string path, bool append = true) : path_(std::move(path)) {
        std::error_code ec;
        const auto p = std::filesystem::path(path_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        file_.open(path_, append ? std::ios::app : std::ios::trunc);
    }

    void log(Level level, const std::string& message, const char* file, int line, const char* function) override {
        if (level < level_) {
            return;
        }
        const auto formatted = format_message(level, message, file, line, function);
        std::lock_guard<std::mutex> lock(mutex_);
        if (not file_.is_open()) {
            return;
        }
        file_ << "[" << level_string(level) << "] " << formatted << "\n";
        file_.flush();
    }

private:
    std::string path_;
    std::mutex mutex_;
    std::ofstream file_;
};

}  // namespace star::base

#endif // __BASEMENT_FILE_LOGGER_H__


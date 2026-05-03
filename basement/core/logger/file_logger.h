#ifndef __BASEMENT_FILE_LOGGER_H__
#define __BASEMENT_FILE_LOGGER_H__

#include "basement/core/exception.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#include "basement/core/logger/logger.h"

namespace star::base {

class FileLogger final : public Logger {
  public:
    explicit FileLogger(std::string path, bool append = true, bool flush_each_line = true) : path_(std::move(path)), flush_each_line_(flush_each_line) {
        std::error_code ec;
        const auto p = std::filesystem::path(path_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        file_.open(path_, append ? std::ios::app : std::ios::trunc);
        if (not file_.is_open()) {
            throw star::excep::RuntimeError("FileLogger open failed: path=", path_);
        }
    }

  protected:
    void write(Level /*level*/, const std::string& formatted) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not file_.is_open()) {
            return;
        }
        file_ << formatted << "\n";
        if (flush_each_line_) {
            file_.flush();
        }
    }

  private:
    std::string path_;
    bool flush_each_line_{true};
    std::mutex mutex_;
    std::ofstream file_;
};

}  // namespace star::base

#endif  // __BASEMENT_FILE_LOGGER_H__

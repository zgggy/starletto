#ifndef __BASEMENT_ROLLING_FILE_LOGGER_H__
#define __BASEMENT_ROLLING_FILE_LOGGER_H__

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "basement/core/logger/logger.h"

namespace star::base {

struct RollingPolicy {
    uint64_t max_file_size{10 * 1024 * 1024};
    uint64_t max_backup_files{5};
    bool flush_each_line{true};
};

class RollingFileLogger final : public Logger {
  public:
    explicit RollingFileLogger(std::string base_path, RollingPolicy policy = {}) : base_path_(std::move(base_path)), policy_(std::move(policy)) { open_current(); }

    void write(Level /*level*/, const std::string& formatted) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not current_) {
            return;
        }
        *current_ << formatted << "\n";
        if (policy_.flush_each_line) {
            current_->flush();
        }
        current_size_ += formatted.size() + 1;
        if (should_roll()) {
            roll();
        }
    }

  private:
    void open_current_trunc() {
        std::error_code ec;
        const auto p = std::filesystem::path(base_path_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        current_ = std::make_unique<std::ofstream>(base_path_, std::ios::trunc);
        current_size_ = 0;
    }

    void open_current() {
        std::error_code ec;
        const auto p = std::filesystem::path(base_path_);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path(), ec);
        }
        current_ = std::make_unique<std::ofstream>(base_path_, std::ios::app);
        current_size_ = std::filesystem::file_size(base_path_, ec);
        if (ec) {
            current_size_ = 0;
        }
    }

    auto should_roll() const -> bool { return current_size_ >= policy_.max_file_size; }

    void roll() {
        current_.reset();
        if (policy_.max_backup_files == 0) {
            open_current_trunc();
            return;
        }
        remove_oldest_backup();
        shift_backups();
        std::error_code ec;
        std::filesystem::rename(base_path_, backup_path(0), ec);
        if (ec) {
            open_current_trunc();
            return;
        }
        open_current();
    }

    void remove_oldest_backup() {
        if (policy_.max_backup_files == 0) {
            return;
        }
        const auto oldest = backup_path(static_cast<int>(policy_.max_backup_files) - 1);
        if (std::filesystem::exists(oldest)) {
            std::filesystem::remove(oldest);
        }
    }

    void shift_backups() {
        if (policy_.max_backup_files == 0) {
            return;
        }
        for (int i = static_cast<int>(policy_.max_backup_files) - 1; i > 0; --i) {
            const auto src = backup_path(i - 1);
            const auto dst = backup_path(i);
            if (std::filesystem::exists(src)) {
                std::filesystem::rename(src, dst);
            }
        }
    }

    auto backup_path(int index) const -> std::string {
        if (index == 0) {
            return base_path_ + ".1";
        }
        return base_path_ + "." + std::to_string(index + 1);
    }

    std::string base_path_;
    RollingPolicy policy_;
    std::mutex mutex_;
    std::unique_ptr<std::ofstream> current_;
    uint64_t current_size_{0};
};

}  // namespace star::base

#endif  // __BASEMENT_ROLLING_FILE_LOGGER_H__

#ifndef __MESSAGE_REPLAYER_H__
#define __MESSAGE_REPLAYER_H__

#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "basement/core/exception.h"
#include "basement/transport/record/message_recorder.h"

namespace star::transport {

class MessageReplayer final {
  public:
    using PublishFn = std::function<void(const std::string& channel_name, const std::string& type_name, int64_t timestamp_ns, const std::vector<uint8_t>& payload)>;

    explicit MessageReplayer(std::string file_path) : file_path_(std::move(file_path)) {}

    void open() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) {
            return;
        }
        file_.open(file_path_, std::ios::binary | std::ios::in);
        if (not file_.is_open()) {
            throw star::excep::RuntimeError("MessageReplayer failed to open file: ", file_path_);
        }
        read_header();
        read_all_entries();
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        file_.close();
        entries_.clear();
    }

    auto message_count() const -> uint64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    auto channels() const -> std::vector<std::string> {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_set<std::string> unique;
        for (const auto& e : entries_) {
            unique.insert(e.channel_name);
        }
        return std::vector<std::string>(unique.begin(), unique.end());
    }

    void play_all(PublishFn publish_fn, double speed = 1.0) {
        std::vector<RecordEntry> entries;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            entries = entries_;
        }
        if (entries.empty() or not publish_fn) {
            return;
        }
        const auto start_ns = entries.front().timestamp_ns;
        const auto play_begin = std::chrono::steady_clock::now();
        for (const auto& entry : entries) {
            const auto elapsed_ns = entry.timestamp_ns - start_ns;
            const auto target_us = static_cast<int64_t>(elapsed_ns / 1000.0 / speed);
            const auto actual_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - play_begin).count();
            if (target_us > actual_us) {
                std::this_thread::sleep_for(std::chrono::microseconds(target_us - actual_us));
            }
            publish_fn(entry.channel_name, entry.type_name, entry.timestamp_ns, entry.payload);
        }
    }

    void play_channel(const std::string& channel, PublishFn publish_fn, double speed = 1.0) {
        std::vector<RecordEntry> filtered;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& e : entries_) {
                if (e.channel_name == channel) {
                    filtered.push_back(e);
                }
            }
        }
        if (filtered.empty() or not publish_fn) {
            return;
        }
        const auto start_ns = filtered.front().timestamp_ns;
        const auto play_begin = std::chrono::steady_clock::now();
        for (const auto& entry : filtered) {
            const auto elapsed_ns = entry.timestamp_ns - start_ns;
            const auto target_us = static_cast<int64_t>(elapsed_ns / 1000.0 / speed);
            const auto actual_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - play_begin).count();
            if (target_us > actual_us) {
                std::this_thread::sleep_for(std::chrono::microseconds(target_us - actual_us));
            }
            publish_fn(entry.channel_name, entry.type_name, entry.timestamp_ns, entry.payload);
        }
    }

  private:
    struct RecordEntry {
        int64_t timestamp_ns{0};
        std::string channel_name;
        std::string type_name;
        std::vector<uint8_t> payload;
    };

    void read_header() {
        char magic[RecordHeader::kMagicSize];
        file_.read(magic, RecordHeader::kMagicSize);
        if (std::memcmp(magic, RecordHeader::kMagic, RecordHeader::kMagicSize) != 0) {
            throw star::excep::RuntimeError("MessageReplayer invalid magic in file: ", file_path_);
        }
        uint32_t version{0};
        file_.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != RecordHeader::kVersion) {
            throw star::excep::RuntimeError("MessageReplayer unsupported version: ", version, ", expected: ", RecordHeader::kVersion);
        }
        uint64_t count{0};
        file_.read(reinterpret_cast<char*>(&count), sizeof(count));
    }

    void read_all_entries() {
        while (file_.peek() != EOF) {
            auto entry = RecordEntry{};
            if (not read_i64(entry.timestamp_ns)) {
                break;
            }
            if (not read_str(entry.channel_name)) {
                break;
            }
            if (not read_str(entry.type_name)) {
                break;
            }
            if (not read_bytes(entry.payload)) {
                break;
            }
            entries_.push_back(std::move(entry));
        }
    }

    bool read_i64(int64_t& out) {
        file_.read(reinterpret_cast<char*>(&out), sizeof(out));
        return static_cast<bool>(file_);
    }

    bool read_str(std::string& out) {
        uint32_t len{0};
        file_.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (not file_ or len > 1024 * 1024) {
            return false;
        }
        out.resize(len);
        if (len > 0) {
            file_.read(out.data(), len);
        }
        return static_cast<bool>(file_);
    }

    bool read_bytes(std::vector<uint8_t>& out) {
        uint32_t len{0};
        file_.read(reinterpret_cast<char*>(&len), sizeof(len));
        if (not file_ or len > 16 * 1024 * 1024) {
            return false;
        }
        out.resize(len);
        if (len > 0) {
            file_.read(reinterpret_cast<char*>(out.data()), len);
        }
        return static_cast<bool>(file_);
    }

    std::string file_path_;
    std::ifstream file_;
    std::vector<RecordEntry> entries_;
    mutable std::mutex mutex_;
};

}  // namespace star::transport

#endif  // __MESSAGE_REPLAYER_H__

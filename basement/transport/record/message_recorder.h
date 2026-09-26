#ifndef __MESSAGE_RECORDER_H__
#define __MESSAGE_RECORDER_H__

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "basement/core/exception.h"

namespace star::transport {

struct RecordHeader {
    static constexpr size_t kMagicSize = 8;
    static constexpr char kMagic[kMagicSize] = {'S', 'T', 'A', 'R', 'R', 'E', 'C', '\0'};
    static constexpr uint32_t kVersion = 1;

    uint32_t version{kVersion};
    uint64_t message_count{0};

    auto encode() const -> std::vector<uint8_t> {
        std::vector<uint8_t> out;
        out.resize(kMagicSize + sizeof(version) + sizeof(message_count));
        auto* p = out.data();
        std::memcpy(p, kMagic, kMagicSize);
        p += kMagicSize;
        std::memcpy(p, &version, sizeof(version));
        p += sizeof(version);
        std::memcpy(p, &message_count, sizeof(message_count));
        return out;
    }
};

struct RecordEntry {
    int64_t timestamp_ns{0};
    std::string channel_name;
    std::string type_name;
    std::vector<uint8_t> payload;

    auto encode() const -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>{};
        auto write_u32 = [&out](uint32_t v) {
            auto bytes = reinterpret_cast<const uint8_t*>(&v);
            out.insert(out.end(), bytes, bytes + sizeof(v));
        };
        auto write_i64 = [&out](int64_t v) {
            auto bytes = reinterpret_cast<const uint8_t*>(&v);
            out.insert(out.end(), bytes, bytes + sizeof(v));
        };
        auto write_str = [&out, &write_u32](const std::string& s) {
            write_u32(static_cast<uint32_t>(s.size()));
            out.insert(out.end(), s.begin(), s.end());
        };
        auto write_payload = [&out, &write_u32](const std::vector<uint8_t>& data) {
            write_u32(static_cast<uint32_t>(data.size()));
            out.insert(out.end(), data.begin(), data.end());
        };
        write_i64(timestamp_ns);
        write_str(channel_name);
        write_str(type_name);
        write_payload(payload);
        return out;
    }
};

class MessageRecorder final {
  public:
    explicit MessageRecorder(std::string file_path) : file_path_(std::move(file_path)) {}

    ~MessageRecorder() { close(); }

    void open() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fp_) {
            return;
        }
        fp_ = fopen(file_path_.c_str(), "wb");
        if (not fp_) {
            throw star::excep::RuntimeError("MessageRecorder failed to open file: ", file_path_);
        }
        auto header = RecordHeader{}.encode();
        fwrite(header.data(), 1, header.size(), fp_);
        fflush(fp_);
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not fp_) {
            return;
        }
        fseek(fp_, static_cast<long>(RecordHeader::kMagicSize + sizeof(uint32_t)), SEEK_SET);
        fwrite(&message_count_, sizeof(message_count_), 1, fp_);
        fclose(fp_);
        fp_ = nullptr;
    }

    void add_channel(const std::string& channel) {
        std::lock_guard<std::mutex> lock(mutex_);
        record_channels_.insert(channel);
    }

    bool should_record(const std::string& channel) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (record_channels_.empty()) {
            return true;
        }
        return record_channels_.count(channel) > 0;
    }

    void record(const std::string& channel_name, const std::string& type_name, int64_t timestamp_ns, const std::vector<uint8_t>& payload) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not fp_) {
            return;
        }
        // 过滤在内部完成：调用方忘了查 should_record() 也不会录进无关通道
        if (not record_channels_.empty() && record_channels_.count(channel_name) == 0) {
            return;
        }
        auto entry = RecordEntry{};
        entry.timestamp_ns = timestamp_ns;
        entry.channel_name = channel_name;
        entry.type_name = type_name;
        entry.payload = payload;
        auto encoded = entry.encode();
        fwrite(encoded.data(), 1, encoded.size(), fp_);
        ++message_count_;
    }

    auto message_count() const -> uint64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return message_count_;
    }

    auto file_path() const -> const std::string& { return file_path_; }

  private:
    std::string file_path_;
    FILE* fp_{nullptr};
    uint64_t message_count_{0};
    std::unordered_set<std::string> record_channels_;
    mutable std::mutex mutex_;
};

}  // namespace star::transport

#endif  // __MESSAGE_RECORDER_H__

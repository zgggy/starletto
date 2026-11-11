#ifndef __MESSAGE_RECORDER_H__
#define __MESSAGE_RECORDER_H__

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "basement/core/exception.h"
#include "basement/core/type_id.h"

namespace star::transport {

// 录制格式 v2（v1 仍可读）。
//
// v1 的问题：条目里只有 C++ 类型名，没有 schema 身份，回放时无法回答
// 「这条字节是哪个版本的契约」；时间戳只有单调时钟，脱离录制机器就无法还原墙钟。
//
// v2 头部增加墙钟/单调时钟双锚点，条目携带 schema name+version+hash：
//   wall_clock_ns(entry) = wall_clock_anchor_ns + (entry.timestamp_ns - steady_anchor_ns)
// 每个条目自带身份比「头部一张通道表」更强：同一通道即使运行中重新声明版本，
// 每条消息的身份也不会被后来的声明覆盖。
struct RecordHeader {
    static constexpr size_t kMagicSize = 8;
    static constexpr char kMagic[kMagicSize] = {'S', 'T', 'A', 'R', 'R', 'E', 'C', '\0'};
    static constexpr uint32_t kVersionV1 = 1;
    static constexpr uint32_t kVersion = 2;

    uint32_t version{kVersion};
    uint64_t message_count{0};
    int64_t wall_clock_anchor_ns{0};
    int64_t steady_anchor_ns{0};

    auto encode() const -> std::vector<uint8_t> {
        std::vector<uint8_t> out;
        out.resize(kEncodedSize);
        auto* p = out.data();
        std::memcpy(p, kMagic, kMagicSize);
        p += kMagicSize;
        std::memcpy(p, &version, sizeof(version));
        p += sizeof(version);
        std::memcpy(p, &message_count, sizeof(message_count));
        p += sizeof(message_count);
        std::memcpy(p, &wall_clock_anchor_ns, sizeof(wall_clock_anchor_ns));
        p += sizeof(wall_clock_anchor_ns);
        std::memcpy(p, &steady_anchor_ns, sizeof(steady_anchor_ns));
        return out;
    }

    static constexpr size_t kEncodedSize = kMagicSize + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(int64_t) + sizeof(int64_t);
    static constexpr size_t kMessageCountOffset = kMagicSize + sizeof(uint32_t);
};

struct RecordEntry {
    int64_t timestamp_ns{0};
    std::string channel_name;
    std::string type_name;
    star::schema::SchemaTypeId schema_type_id{};
    std::vector<uint8_t> payload;

    auto encode() const -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>{};
        auto write_u32 = [&out](uint32_t v) {
            auto bytes = reinterpret_cast<const uint8_t*>(&v);
            out.insert(out.end(), bytes, bytes + sizeof(v));
        };
        auto write_u64 = [&out](uint64_t v) {
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
        write_str(schema_type_id.name);
        write_i64(schema_type_id.version);
        write_u64(schema_type_id.hash);
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
        header_ = RecordHeader{};
        header_.wall_clock_anchor_ns = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                                  std::chrono::system_clock::now().time_since_epoch())
                                                                  .count());
        header_.steady_anchor_ns = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                                std::chrono::steady_clock::now().time_since_epoch())
                                                                .count());
        const auto encoded = header_.encode();
        fwrite(encoded.data(), 1, encoded.size(), fp_);
        fflush(fp_);
    }

    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (not fp_) {
            return;
        }
        fseek(fp_, static_cast<long>(RecordHeader::kMessageCountOffset), SEEK_SET);
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

    void record(const std::string& channel_name,
                const std::string& type_name,
                const star::schema::SchemaTypeId& schema_type_id,
                int64_t timestamp_ns,
                const std::vector<uint8_t>& payload) {
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
        entry.schema_type_id = schema_type_id;
        entry.payload = payload;
        auto encoded = entry.encode();
        fwrite(encoded.data(), 1, encoded.size(), fp_);
        ++message_count_;
    }

    // 兼容旧调用：不携带 schema 身份时写空身份（回放侧按 hash 未知处理）
    void record(const std::string& channel_name, const std::string& type_name, int64_t timestamp_ns, const std::vector<uint8_t>& payload) {
        record(channel_name, type_name, star::schema::SchemaTypeId{}, timestamp_ns, payload);
    }

    auto message_count() const -> uint64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return message_count_;
    }

    auto wall_clock_anchor_ns() const -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return header_.wall_clock_anchor_ns;
    }

    auto steady_anchor_ns() const -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return header_.steady_anchor_ns;
    }

    auto file_path() const -> const std::string& { return file_path_; }

  private:
    std::string file_path_;
    FILE* fp_{nullptr};
    RecordHeader header_{};
    uint64_t message_count_{0};
    std::unordered_set<std::string> record_channels_;
    mutable std::mutex mutex_;
};

}  // namespace star::transport

#endif  // __MESSAGE_RECORDER_H__

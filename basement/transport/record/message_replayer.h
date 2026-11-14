#ifndef __MESSAGE_REPLAYER_H__
#define __MESSAGE_REPLAYER_H__

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "basement/core/exception.h"
#include "basement/transport/record/message_recorder.h"

namespace star::transport {

class MessageReplayer final {
  public:
    struct RecordEntry {
        int64_t timestamp_ns{0};
        std::string channel_name;
        std::string type_name;
        star::schema::SchemaTypeId schema_type_id{};
        std::vector<uint8_t> payload;
    };

    // 按通道聚合的录制摘要：哪些通道、什么类型、出现过哪些 schema 版本
    struct ChannelSummary {
        std::string channel_name;
        std::string type_name;
        std::vector<int64_t> versions;
        std::vector<uint64_t> hashes;
        uint64_t message_count{0};
        int64_t first_timestamp_ns{0};
        int64_t last_timestamp_ns{0};
    };

    struct CompatibilityIssue {
        std::string channel_name;
        std::string schema_name;
        int64_t recorded_version{0};
        uint64_t recorded_hash{0};
        int64_t expected_version{0};
        uint64_t expected_hash{0};
        std::string message;
    };

    struct Validation {
        std::vector<CompatibilityIssue> issues;
        auto ok() const -> bool { return issues.empty(); }
    };

    // 旧接口：只带类型名，不携带 schema 身份
    using WriteFn = std::function<void(const std::string& channel_name, const std::string& type_name, int64_t timestamp_ns, const std::vector<uint8_t>& payload)>;
    // 推荐接口：完整条目，含 schema name/version/hash
    using EntryFn = std::function<void(const RecordEntry&)>;

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

    auto file_version() const -> uint32_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return file_version_;
    }

    auto channels() const -> std::vector<std::string> {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_set<std::string> unique;
        for (const auto& e : entries_) {
            unique.insert(e.channel_name);
        }
        return std::vector<std::string>(unique.begin(), unique.end());
    }

    // 录制头里的墙钟锚点；v1 文件没有，返回 0
    auto wall_clock_anchor_ns() const -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return header_.wall_clock_anchor_ns;
    }

    auto steady_anchor_ns() const -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return header_.steady_anchor_ns;
    }

    // 单调时间戳 → 墙钟纳秒。v1 文件没有锚点，只能原样返回（不假装知道）
    auto wall_clock_ns(const RecordEntry& entry) const -> int64_t {
        std::lock_guard<std::mutex> lock(mutex_);
        if (header_.wall_clock_anchor_ns == 0) {
            return entry.timestamp_ns;
        }
        return header_.wall_clock_anchor_ns + (entry.timestamp_ns - header_.steady_anchor_ns);
    }

    // 一次性快照（回放前校验、发布器预扫描通道都要用）
    auto entries() const -> std::vector<RecordEntry> {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

    auto summaries() const -> std::vector<ChannelSummary> {
        std::lock_guard<std::mutex> lock(mutex_);
        return build_summaries(entries_);
    }

    // 回放前校验：把录制里的 schema 身份与本地期望的（编译期）身份逐通道比较。
    // 同 name、version/hash 不同 → 报告；任一方 hash=0（未知）→ 不阻断，只由解码策略兜底。
    auto validate_against(const std::vector<star::schema::SchemaTypeId>& expected) const -> Validation {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<std::string, star::schema::SchemaTypeId> by_name;
        for (const auto& id : expected) {
            by_name.emplace(id.name, id);
        }

        Validation out;
        std::unordered_set<std::string> reported;
        for (const auto& entry : entries_) {
            const auto& recorded = entry.schema_type_id;
            if (recorded.name.empty()) {
                continue;
            }
            const auto key = entry.channel_name + "#" + recorded.name + "#" + std::to_string(recorded.version) + "#" + std::to_string(recorded.hash);
            if (not reported.insert(key).second) {
                continue;
            }
            const auto it = by_name.find(recorded.name);
            if (it == by_name.end()) {
                out.issues.push_back(CompatibilityIssue{
                    .channel_name = entry.channel_name,
                    .schema_name = recorded.name,
                    .recorded_version = recorded.version,
                    .recorded_hash = recorded.hash,
                    .expected_version = 0,
                    .expected_hash = 0,
                    .message = "recorded schema is not registered locally",
                });
                continue;
            }
            const auto& expected_id = it->second;
            if (recorded.version == expected_id.version and
                (recorded.hash == 0 or expected_id.hash == 0 or recorded.hash == expected_id.hash)) {
                continue;
            }
            out.issues.push_back(CompatibilityIssue{
                .channel_name = entry.channel_name,
                .schema_name = recorded.name,
                .recorded_version = recorded.version,
                .recorded_hash = recorded.hash,
                .expected_version = expected_id.version,
                .expected_hash = expected_id.hash,
                .message = "recorded schema version differs from the locally compiled schema",
            });
        }
        return out;
    }

    void play_all(WriteFn write_fn, double speed = 1.0) {
        play_all_entries(
            [&write_fn](const RecordEntry& entry) {
                write_fn(entry.channel_name, entry.type_name, entry.timestamp_ns, entry.payload);
            },
            speed);
    }

    void play_all_entries(EntryFn write_fn, double speed = 1.0) {
        std::vector<RecordEntry> entries;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            entries = entries_;
        }
        if (entries.empty() or not write_fn) {
            return;
        }
        const auto start_ns = entries.front().timestamp_ns;
        const auto play_begin = std::chrono::steady_clock::now();
        for (const auto& entry : entries) {
            pace_to(entry.timestamp_ns - start_ns, play_begin, speed);
            write_fn(entry);
        }
    }

    void play_channel(const std::string& channel, WriteFn write_fn, double speed = 1.0) {
        play_channel_entries(
            channel,
            [&write_fn](const RecordEntry& entry) {
                write_fn(entry.channel_name, entry.type_name, entry.timestamp_ns, entry.payload);
            },
            speed);
    }

    void play_channel_entries(const std::string& channel, EntryFn write_fn, double speed = 1.0) {
        std::vector<RecordEntry> filtered;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& e : entries_) {
                if (e.channel_name == channel) {
                    filtered.push_back(e);
                }
            }
        }
        if (filtered.empty() or not write_fn) {
            return;
        }
        const auto start_ns = filtered.front().timestamp_ns;
        const auto play_begin = std::chrono::steady_clock::now();
        for (const auto& entry : filtered) {
            pace_to(entry.timestamp_ns - start_ns, play_begin, speed);
            write_fn(entry);
        }
    }

  private:
    static void pace_to(int64_t elapsed_ns, std::chrono::steady_clock::time_point play_begin, double speed) {
        const auto target_us = static_cast<int64_t>(elapsed_ns / 1000.0 / speed);
        const auto actual_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - play_begin).count();
        if (target_us > actual_us) {
            std::this_thread::sleep_for(std::chrono::microseconds(target_us - actual_us));
        }
    }

    static auto build_summaries(const std::vector<RecordEntry>& entries) -> std::vector<ChannelSummary> {
        std::vector<ChannelSummary> out;
        std::unordered_map<std::string, size_t> by_channel;
        for (const auto& entry : entries) {
            auto it = by_channel.find(entry.channel_name);
            if (it == by_channel.end()) {
                auto summary = ChannelSummary{};
                summary.channel_name = entry.channel_name;
                summary.type_name = entry.type_name;
                summary.first_timestamp_ns = entry.timestamp_ns;
                summary.last_timestamp_ns = entry.timestamp_ns;
                out.push_back(std::move(summary));
                it = by_channel.emplace(entry.channel_name, out.size() - 1).first;
            }
            auto& summary = out[it->second];
            ++summary.message_count;
            summary.last_timestamp_ns = entry.timestamp_ns;
            if (not entry.schema_type_id.name.empty()) {
                if (std::find(summary.versions.begin(), summary.versions.end(), entry.schema_type_id.version) == summary.versions.end()) {
                    summary.versions.push_back(entry.schema_type_id.version);
                }
                if (std::find(summary.hashes.begin(), summary.hashes.end(), entry.schema_type_id.hash) == summary.hashes.end()) {
                    summary.hashes.push_back(entry.schema_type_id.hash);
                }
            }
        }
        return out;
    }

    void read_header() {
        char magic[RecordHeader::kMagicSize];
        file_.read(magic, RecordHeader::kMagicSize);
        if (std::memcmp(magic, RecordHeader::kMagic, RecordHeader::kMagicSize) != 0) {
            throw star::excep::RuntimeError("MessageReplayer invalid magic in file: ", file_path_);
        }
        file_.read(reinterpret_cast<char*>(&file_version_), sizeof(file_version_));
        if (file_version_ != RecordHeader::kVersionV1 && file_version_ != RecordHeader::kVersion) {
            throw star::excep::RuntimeError("MessageReplayer unsupported version: ", file_version_, ", supported: 1,2");
        }
        uint64_t count{0};
        file_.read(reinterpret_cast<char*>(&count), sizeof(count));
        if (file_version_ >= RecordHeader::kVersion) {
            file_.read(reinterpret_cast<char*>(&header_.wall_clock_anchor_ns), sizeof(header_.wall_clock_anchor_ns));
            file_.read(reinterpret_cast<char*>(&header_.steady_anchor_ns), sizeof(header_.steady_anchor_ns));
        }
        header_.version = file_version_;
        header_.message_count = count;
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
            if (file_version_ >= RecordHeader::kVersion) {
                if (not read_str(entry.schema_type_id.name)) {
                    break;
                }
                if (not read_i64(entry.schema_type_id.version)) {
                    break;
                }
                if (not read_u64(entry.schema_type_id.hash)) {
                    break;
                }
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

    bool read_u64(uint64_t& out) {
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
    uint32_t file_version_{0};
    RecordHeader header_{};
    std::vector<RecordEntry> entries_;
    mutable std::mutex mutex_;
};

}  // namespace star::transport

#endif  // __MESSAGE_REPLAYER_H__

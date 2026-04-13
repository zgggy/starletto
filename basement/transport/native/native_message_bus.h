#ifndef __NATIVE_MESSAGE_BUS_H__
#define __NATIVE_MESSAGE_BUS_H__

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/record/message_recorder.h"
#include "libraries/common/schema/schema_value.h"

namespace star::transport {

class NativeMessageBus final : public MessageBus, public std::enable_shared_from_this<NativeMessageBus> {
    friend class NativeWriter;
    friend class InprocAckWriter;
    friend class NativeReader;

  public:
    explicit NativeMessageBus(std::shared_ptr<ChannelRegistry> channel_registry = nullptr) : channel_registry_(std::move(channel_registry)) {}
    ~NativeMessageBus() override = default;

    void declare_channel(const ChannelDescriptor& descriptor) override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("NativeMessageBus declare_channel failed, empty channel name");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto& channel = channels_[descriptor.name];
        if (channel.descriptor.name.empty()) {
            channel.descriptor = descriptor;
            return;
        }
        if (not descriptor.type_name.empty() and not channel.descriptor.type_name.empty() and descriptor.type_name != channel.descriptor.type_name) {
            throw star::excep::RuntimeError("NativeMessageBus declare_channel failed, type mismatch for channel: ", descriptor.name, ", existing=", channel.descriptor.type_name,
                                            ", new=", descriptor.type_name);
        }
        if (channel.descriptor.type_name.empty() and not descriptor.type_name.empty()) {
            channel.descriptor.type_name = descriptor.type_name;
        }
        if (channel.descriptor.schema_type_id.empty()) {
            channel.descriptor.schema_type_id = descriptor.schema_type_id;
            return;
        }
        if (not descriptor.schema_type_id.empty() and channel.descriptor.schema_type_id != descriptor.schema_type_id) {
            throw star::excep::RuntimeError("NativeMessageBus declare_channel failed, schema mismatch for channel: ", descriptor.name, ", existing=", channel.descriptor.schema_type_id.name,
                                            ", new=", descriptor.schema_type_id.name);
        }
    }

    auto create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> override;
    auto create_ack_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<AckWriter> override;
    auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> override;

    auto dump_stats_lines() const -> std::vector<std::string> override {
        auto out = std::vector<std::string>{};
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [channel_name, channel] : channels_) {
            if (channel.stats.publish_count == 0 && channel.stats.deliver_count == 0) {
                continue;
            }
            const auto hz = compute_hz(channel.stats);
            const auto p99 = compute_percentile(channel.stats.latency_samples, 0.99);
            const auto p999 = compute_percentile(channel.stats.latency_samples, 0.999);
            out.emplace_back("transport.native channel=" + channel_name + " publish=" + std::to_string(channel.stats.publish_count) + " deliver=" + std::to_string(channel.stats.deliver_count) +
                             " dropped=" + std::to_string(channel.stats.dropped_count) + " payload_bytes_total=" + std::to_string(channel.stats.payload_bytes_total) +
                             " payload_bytes_last=" + std::to_string(channel.stats.payload_bytes_last) + " latency_avg_us=" + std::to_string(compute_avg_latency_us(channel.stats)) +
                             " latency_p99_us=" + std::to_string(p99) + " latency_p999_us=" + std::to_string(p999) + " latency_max_us=" + std::to_string(channel.stats.latency_max_us) +
                             " max_queue_depth=" + std::to_string(channel.stats.max_queue_depth) + " avg_hz=" + format_rate(hz));
        }
        return out;
    }

    void enable_recorder(const std::string& file_path) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (recorder_) {
            return;
        }
        recorder_ = std::make_unique<MessageRecorder>(file_path);
        recorder_->open();
        for (const auto& channel : record_channels_) {
            recorder_->add_channel(channel);
        }
    }

    void disable_recorder() {
        std::lock_guard<std::mutex> lock(mutex_);
        recorder_.reset();
    }

    void enable_record_channel(const std::string& channel) override {
        std::lock_guard<std::mutex> lock(mutex_);
        record_channels_.insert(channel);
        if (recorder_) {
            recorder_->add_channel(channel);
        }
    }

    auto reader_history_size(const std::string& channel_name) const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return 0;
        }
        size_t total = 0;
        for (const auto& [_, history] : it->second.reader_history) {
            total += history.size();
        }
        return total;
    }

    auto peek_latest(const std::string& channel_name) const -> std::optional<MessageView> {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return std::nullopt;
        }
        MessageView latest;
        bool found = false;
        for (const auto& [_, history] : it->second.reader_history) {
            if (not history.empty()) {
                latest = history.back();
                found = true;
            }
        }
        return found ? std::optional<MessageView>{latest} : std::nullopt;
    }

    auto declared_channels() const -> std::vector<std::string> override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto out = std::vector<std::string>{};
        out.reserve(channels_.size());
        for (const auto& [name, _] : channels_) {
            out.push_back(name);
        }
        return out;
    }

  private:
    static constexpr size_t kMaxLatencySamples = 1000;

    struct ChannelStats {
        uint64_t publish_count{0};
        uint64_t deliver_count{0};
        uint64_t dropped_count{0};
        uint64_t payload_bytes_total{0};
        uint64_t payload_bytes_last{0};
        uint64_t latency_total_us{0};
        uint64_t latency_max_us{0};
        uint64_t max_queue_depth{0};
        std::vector<uint64_t> latency_samples;
        std::chrono::steady_clock::time_point first_publish_tp{};
        std::chrono::steady_clock::time_point last_publish_tp{};
    };

    struct ChannelState {
        ChannelDescriptor descriptor;
        std::unordered_map<uint64_t, Reader::Callback> readers;
        std::unordered_map<uint64_t, std::deque<MessageView>> reader_history;
        ChannelStats stats;
    };

    static auto compute_percentile(const std::vector<uint64_t>& samples, double p) -> uint64_t {
        if (samples.empty()) {
            return 0;
        }
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const auto idx = static_cast<size_t>(std::ceil(p * sorted.size())) - 1;
        return sorted[std::min(idx, sorted.size() - 1)];
    }

    void publish_internal(const std::string& channel_name, const MessageView& message) {
        std::vector<std::pair<uint64_t, Reader::Callback>> reader_callbacks;
        const auto dispatch_begin = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                return;
            }
            if (not it->second.descriptor.schema_type_id.empty() and not message.schema_type_id.empty() and it->second.descriptor.schema_type_id != message.schema_type_id) {
                throw star::excep::RuntimeError("NativeMessageBus publish failed, schema mismatch for channel: ", channel_name, ", existing=", it->second.descriptor.schema_type_id.name,
                                                ", message=", message.schema_type_id.name);
            }
            const auto& options = it->second.descriptor.options;
            reader_callbacks.reserve(it->second.readers.size());
            for (const auto& [reader_id, cb] : it->second.readers) {
                if (options.depth > 0) {
                    auto& history = it->second.reader_history[reader_id];
                    if (options.drop_policy == DropPolicy::KEEP_LAST) {
                        history.push_back(message);
                        while (history.size() > options.depth) {
                            history.pop_front();
                            it->second.stats.dropped_count += 1;
                        }
                    } else {
                        if (history.size() >= options.depth) {
                            it->second.stats.dropped_count += 1;
                        } else {
                            history.push_back(message);
                        }
                    }
                    it->second.stats.max_queue_depth = std::max(it->second.stats.max_queue_depth, history.size());
                }
                reader_callbacks.emplace_back(reader_id, cb);
            }
            auto payload_size = estimate_payload_size(message);
            auto& stats = it->second.stats;
            const auto now = std::chrono::steady_clock::now();
            if (stats.publish_count == 0) {
                stats.first_publish_tp = now;
            }
            stats.last_publish_tp = now;
            stats.publish_count += 1;
            stats.deliver_count += reader_callbacks.size();
            stats.payload_bytes_total += payload_size;
            stats.payload_bytes_last = payload_size;
        }

        for (const auto& [_, cb] : reader_callbacks) {
            cb(message);
        }
        if (recorder_ and should_record_channel_(channel_name)) {
            auto now_ns = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
            std::string type_name;
            {
                auto it = channels_.find(channel_name);
                if (it != channels_.end()) {
                    type_name = it->second.descriptor.type_name;
                }
            }
            auto payload = message.encode_binary ? message.encode_binary() : std::vector<uint8_t>{};
            recorder_->record(channel_name, type_name, now_ns, payload);
        }
        const auto latency_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - dispatch_begin).count());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it != channels_.end()) {
                auto& stats = it->second.stats;
                stats.latency_total_us += latency_us;
                stats.latency_max_us = std::max(stats.latency_max_us, latency_us);
                if (stats.latency_samples.size() < kMaxLatencySamples) {
                    stats.latency_samples.push_back(latency_us);
                } else {
                    stats.latency_samples[stats.publish_count % kMaxLatencySamples] = latency_us;
                }
            }
        }
    }

    void erase_reader(const std::string& channel_name, uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return;
        }
        it->second.readers.erase(id);
        it->second.reader_history.erase(id);
    }

    static auto estimate_payload_size(const MessageView& message) -> uint64_t {
        if (message.encode_binary) {
            return message.encode_binary().size();
        }
        if (const auto conf = message.data.as<star::schema::SchemaValue>()) {
            return star::schema::dump_schema_text(*conf).size();
        }
        return 0;
    }

    static auto compute_hz(const ChannelStats& stats) -> double {
        if (stats.publish_count <= 1) {
            return static_cast<double>(stats.publish_count);
        }
        const auto seconds = std::chrono::duration<double>(stats.last_publish_tp - stats.first_publish_tp).count();
        if (seconds <= 0.0) {
            return static_cast<double>(stats.publish_count);
        }
        return static_cast<double>(stats.publish_count - 1) / seconds;
    }

    static auto compute_avg_latency_us(const ChannelStats& stats) -> uint64_t {
        if (stats.publish_count == 0) {
            return 0;
        }
        return stats.latency_total_us / stats.publish_count;
    }

    static auto format_rate(double hz) -> std::string {
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%.2f", hz);
        return std::string(buf);
    }

    void maybe_register_writer(const std::string& channel) {
        if (channel_registry_ == nullptr) {
            return;
        }
        const auto local_group = channel_registry_->local_group();
        if (local_group.empty()) {
            return;
        }
        channel_registry_->register_writer(channel, local_group);
    }

    void maybe_register_reader(const std::string& channel) {
        if (channel_registry_ == nullptr) {
            return;
        }
        const auto local_group = channel_registry_->local_group();
        if (local_group.empty()) {
            return;
        }
        channel_registry_->register_reader(channel, local_group);
    }

    mutable std::mutex mutex_;
    std::shared_ptr<ChannelRegistry> channel_registry_;
    std::unordered_map<std::string, ChannelState> channels_;
    std::atomic<uint64_t> next_sub_id_{1};
    std::unique_ptr<MessageRecorder> recorder_;
    std::unordered_set<std::string> record_channels_;

    auto should_record_channel_(const std::string& channel) const -> bool {
        if (record_channels_.empty()) {
            return true;
        }
        return record_channels_.count(channel) > 0;
    }
};

#include "basement/transport/native/native_reader.h"
#include "basement/transport/native/native_writer.h"

inline auto NativeMessageBus::create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> {
    declare_channel(descriptor);
    maybe_register_writer(descriptor.name);
    return std::make_shared<NativeWriter>(this->shared_from_this(), descriptor.name);
}

inline auto NativeMessageBus::create_ack_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<AckWriter> {
    declare_channel(descriptor);
    maybe_register_writer(descriptor.name);
    return std::make_shared<InprocAckWriter>(this->shared_from_this(), descriptor.name, descriptor.options.ack_timeout_ms);
}

inline auto NativeMessageBus::create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> {
    if (not callback) {
        throw star::excep::RuntimeError("NativeMessageBus create_reader failed, empty callback");
    }
    declare_channel(descriptor);
    maybe_register_reader(descriptor.name);

    const auto id = next_sub_id_.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& channel = channels_[descriptor.name];
        channel.readers.emplace(id, std::move(callback));
    }
    return std::make_shared<NativeReader>(this->shared_from_this(), descriptor.name, id);
}

}  // namespace star::transport

#endif  // __NATIVE_MESSAGE_BUS_H__

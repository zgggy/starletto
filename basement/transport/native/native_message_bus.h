#ifndef __NATIVE_MESSAGE_BUS_H__
#define __NATIVE_MESSAGE_BUS_H__

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/common/channel_stats.h"
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
            out.emplace_back(format_stats_line("native", channel_name, channel.stats));
        }
        return out;
    }

    auto channel_stats(const std::string& channel_name) const -> std::optional<ChannelStatsSnapshot> override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return std::nullopt;
        }
        if (it->second.stats.publish_count == 0 && it->second.stats.deliver_count == 0) {
            return std::nullopt;
        }
        return make_stats_snapshot(channel_name, it->second.stats);
    }

    auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot> override {
        auto out = std::vector<ChannelStatsSnapshot>{};
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [channel_name, channel] : channels_) {
            if (channel.stats.publish_count == 0 && channel.stats.deliver_count == 0) {
                continue;
            }
            out.push_back(make_stats_snapshot(channel_name, channel.stats));
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
    struct ChannelState {
        ChannelDescriptor descriptor;
        std::unordered_map<uint64_t, Reader::Callback> readers;
        std::unordered_map<uint64_t, std::deque<MessageView>> reader_history;
        ChannelStats stats;
    };

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
                            it->second.stats.record_drop(1);
                        }
                    } else {
                        if (history.size() >= options.depth) {
                            it->second.stats.record_drop(1);
                        } else {
                            history.push_back(message);
                        }
                    }
                    it->second.stats.update_queue_depth(history.size());
                }
                reader_callbacks.emplace_back(reader_id, cb);
            }
            auto payload_size = estimate_payload_size(message);
            it->second.stats.record_publish(payload_size);
            it->second.stats.record_deliver(reader_callbacks.size());
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
                it->second.stats.record_latency(latency_us);
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

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
#include "basement/transport/common/qos_manager.h"
#include "basement/transport/record/message_recorder.h"
#include "basement/transport/shm/shm_views.h"

namespace star::transport {

class NativeMessageBus final : public MessageBus, public RecordableBus, public std::enable_shared_from_this<NativeMessageBus> {
    friend class NativeWriter;
    friend class InprocSyncWriter;
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
    auto create_delivery_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<DeliveryWriter> override;
    auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> override;

    auto dump_stats_lines() const -> std::vector<std::string> override {
        auto out = std::vector<std::string>{};
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [channel_name, channel] : channels_) {
            if (channel.stats.write_count == 0 && channel.stats.deliver_count == 0) {
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
        if (it->second.stats.write_count == 0 && it->second.stats.deliver_count == 0) {
            return std::nullopt;
        }
        return make_stats_snapshot(channel_name, it->second.stats);
    }

    auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot> override {
        auto out = std::vector<ChannelStatsSnapshot>{};
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [channel_name, channel] : channels_) {
            if (channel.stats.write_count == 0 && channel.stats.deliver_count == 0) {
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
        recorder_ = std::make_shared<MessageRecorder>(file_path);
        recorder_->open();
        for (const auto& channel : record_channels_) {
            recorder_->add_channel(channel);
        }
    }

    void disable_recorder() override {
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

    void disable_record_channel(const std::string& channel) override {
        std::lock_guard<std::mutex> lock(mutex_);
        record_channels_.erase(channel);
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

    auto peek_latest(const std::string& channel_name) const -> std::optional<MessageView> override {
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

    void write_internal(const std::string& channel_name, const MessageView& message, const MessageSerializer* serializer) {
        std::vector<std::pair<uint64_t, Reader::Callback>> reader_callbacks;
        std::shared_ptr<MessageRecorder> recorder;
        bool should_record = false;
        std::string record_type_name;
        const auto dispatch_begin = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                return;
            }
            if (not it->second.descriptor.schema_type_id.empty() and not message.schema_type_id.empty() and it->second.descriptor.schema_type_id != message.schema_type_id) {
                throw star::excep::RuntimeError("NativeMessageBus write failed, schema mismatch for channel: ", channel_name, ", existing=", it->second.descriptor.schema_type_id.name,
                                                ", message=", message.schema_type_id.name);
            }
            const auto& options = it->second.descriptor.options;
            reader_callbacks.reserve(it->second.readers.size());
            for (const auto& [reader_id, cb] : it->second.readers) {
                if (options.depth > 0) {
                    auto& history = it->second.reader_history[reader_id];
                    auto result = apply_qos(history, message, options, it->second.stats);
                    if (not result.should_deliver) {
                        continue;
                    }
                }
                reader_callbacks.emplace_back(reader_id, cb);
            }
            auto payload_size = estimate_payload_size(message, serializer);
            it->second.stats.record_write(payload_size);
            it->second.stats.record_deliver(reader_callbacks.size());

            recorder = recorder_;
            should_record = recorder != nullptr && (record_channels_.empty() || record_channels_.count(channel_name) > 0);
            record_type_name = it->second.descriptor.type_name;
        }

        for (const auto& [_, cb] : reader_callbacks) {
            cb(message);
        }

        if (should_record) {
            const auto now_ns = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                                                         .count());
            auto payload = encode_payload(serializer);
            recorder->record(channel_name, record_type_name, now_ns, payload);
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

    static auto estimate_payload_size(const MessageView& message, const MessageSerializer* serializer) -> uint64_t {
        if (serializer != nullptr) {
            if (serializer->payload_size_hint > 0) {
                return serializer->payload_size_hint;
            }
        }
        if (const auto slot = message.payload.as<star::transport::ShmSlotView>()) {
            return slot->size;
        }
        return 0;
    }

    static auto encode_payload(const MessageSerializer* serializer) -> std::vector<uint8_t> {
        if (serializer == nullptr || !serializer->encode_binary) {
            return {};
        }
        return serializer->encode_binary();
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
    std::atomic<uint64_t> next_reader_id_{1};
    std::shared_ptr<MessageRecorder> recorder_;
    std::unordered_set<std::string> record_channels_;
};

#include "basement/transport/native/native_reader.h"
#include "basement/transport/native/native_writer.h"

inline auto NativeMessageBus::create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> {
    declare_channel(descriptor);
    maybe_register_writer(descriptor.name);
    return std::make_shared<NativeWriter>(this->shared_from_this(), descriptor.name);
}

inline auto NativeMessageBus::create_delivery_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<DeliveryWriter> {
    declare_channel(descriptor);
    maybe_register_writer(descriptor.name);
    return std::make_shared<InprocSyncWriter>(this->shared_from_this(), descriptor.name);
}

inline auto NativeMessageBus::create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> {
    if (not callback) {
        throw star::excep::RuntimeError("NativeMessageBus create_reader failed, empty callback");
    }
    declare_channel(descriptor);
    maybe_register_reader(descriptor.name);

    const auto id = next_reader_id_.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& channel = channels_[descriptor.name];
        channel.readers.emplace(id, std::move(callback));
    }
    return std::make_shared<NativeReader>(this->shared_from_this(), descriptor.name, id);
}

}  // namespace star::transport

#endif  // __NATIVE_MESSAGE_BUS_H__

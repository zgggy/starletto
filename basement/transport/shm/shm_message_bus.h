#ifndef __SHM_MESSAGE_BUS_H__
#define __SHM_MESSAGE_BUS_H__

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/common/channel_stats.h"
#include "basement/transport/common/qos_manager.h"
#include "basement/transport/shm/shm_channel.h"
#include "basement/transport/shm/shm_views.h"

namespace star::transport {

class ShmMessageBus final : public MessageBus, public std::enable_shared_from_this<ShmMessageBus> {
    class ShmWriter final : public Writer {
      public:
        ShmWriter(std::shared_ptr<ShmMessageBus> bus, std::string channel_name) : bus_(std::move(bus)), channel_name_(std::move(channel_name)) {}

        auto write(const MessageView& message) -> WriteResult override {
            auto bus = bus_.lock();
            if (not bus) {
                auto out = WriteResult{};
                out.status = WriteStatus::BUS_EXPIRED;
                return out;
            }
            return bus->write_internal(channel_name_, message, nullptr);
        }

        auto write_serialized(const SerializedMessage& message) -> WriteResult override {
            auto bus = bus_.lock();
            if (not bus) {
                auto out = WriteResult{};
                out.status = WriteStatus::BUS_EXPIRED;
                return out;
            }
            return bus->write_internal(channel_name_, message.view, &message.serializer);
        }

      private:
        std::weak_ptr<ShmMessageBus> bus_;
        std::string channel_name_;
    };

    class ShmReader final : public Reader {
      public:
        ShmReader(std::shared_ptr<ShmMessageBus> bus, std::string channel_name, uint64_t id, std::shared_ptr<ShmChannel> channel)
            : bus_(std::move(bus)), channel_name_(std::move(channel_name)), id_(id), channel_(std::move(channel)), baseline_seq_(channel_ ? channel_->current_seq() : 0) {
            // 基线必须在启动轮询线程之前捕获：若放到 poll_loop 里取，
            // 订阅建立后、线程跑起来前发布的消息会被误判为「已见过」而丢失
            poll_thread_ = std::thread([this]() { poll_loop(); });
        }

        ~ShmReader() override {
            stop_.store(true, std::memory_order_relaxed);
            if (channel_) {
                channel_->wake_waiters();
            }
            if (poll_thread_.joinable()) {
                poll_thread_.join();
            }
            auto bus = bus_.lock();
            if (bus) {
                bus->erase_reader(channel_name_, id_);
            }
        }

        auto peek_latest() const -> std::optional<MessageView> override {
            auto bus = bus_.lock();
            if (not bus) {
                return std::nullopt;
            }
            return bus->peek_latest(channel_name_);
        }

        auto has_data() const -> bool override {
            return peek_latest().has_value();
        }

      private:
        void poll_loop() {
            if (not channel_) {
                return;
            }
            const auto wait_ms = resolve_wait_ms();
            uint64_t last_seq = baseline_seq_;
            while (not stop_.load(std::memory_order_relaxed)) {
                const auto r = channel_->read(&last_seq);
                if (r.has_value()) {
                    auto bus = bus_.lock();
                    if (bus) {
                        if (r->dropped > 0) {
                            bus->record_drop(channel_name_, r->dropped);
                        }
                        bus->record_latency(channel_name_, r->write_ns);
                        bus->deliver_to_reader(channel_name_, id_, r->slot, r->size, r->seq);
                    }
                    continue;
                }
                channel_->wait_for_update(last_seq, wait_ms);
            }
        }

        static auto resolve_wait_ms() -> int64_t {
            static const int64_t cached = []() -> int64_t {
                if (const auto* v = std::getenv("STAR_SHM_WAIT_MS"); v != nullptr && std::string(v).size() > 0) {
                    try {
                        return std::max<int64_t>(1, std::stoll(v));
                    } catch (...) {
                    }
                }
                return 100;
            }();
            return cached;
        }

        std::weak_ptr<ShmMessageBus> bus_;
        std::string channel_name_;
        uint64_t id_{0};
        std::shared_ptr<ShmChannel> channel_;
        uint64_t baseline_seq_{0};
        std::atomic<bool> stop_{false};
        std::thread poll_thread_;
    };

  public:
    explicit ShmMessageBus(std::shared_ptr<ChannelRegistry> channel_registry = nullptr) : channel_registry_(std::move(channel_registry)) {}
    ~ShmMessageBus() override = default;

    void declare_channel(const ChannelDescriptor& descriptor) override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("ShmMessageBus declare_channel failed, empty channel name");
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto& channel = channels_[descriptor.name];
        if (channel.descriptor.name.empty()) {
            channel.descriptor = descriptor;
        } else {
            if (not descriptor.type_name.empty() && not channel.descriptor.type_name.empty() && descriptor.type_name != channel.descriptor.type_name) {
                throw star::excep::RuntimeError("ShmMessageBus declare_channel failed, type mismatch for channel: ", descriptor.name, ", existing=", channel.descriptor.type_name,
                                                ", new=", descriptor.type_name);
            }
            if (channel.descriptor.type_name.empty() && not descriptor.type_name.empty()) {
                channel.descriptor.type_name = descriptor.type_name;
            }
            if (channel.descriptor.schema_type_id.empty()) {
                channel.descriptor.schema_type_id = descriptor.schema_type_id;
            } else if (not descriptor.schema_type_id.empty() && channel.descriptor.schema_type_id != descriptor.schema_type_id) {
                throw star::excep::RuntimeError("ShmMessageBus declare_channel failed, schema mismatch for channel: ", descriptor.name, ", existing=", channel.descriptor.schema_type_id.name,
                                                ", new=", descriptor.schema_type_id.name);
            }
        }
        if (not channel.channel) {
            channel.channel = std::make_shared<ShmChannel>(make_shm_name(descriptor.name), resolve_capacity());
        }
    }

    auto create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> override {
        declare_channel(descriptor);
        maybe_register_writer(descriptor.name);
        return std::make_shared<ShmWriter>(this->shared_from_this(), descriptor.name);
    }

    auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> override {
        if (not callback) {
            throw star::excep::RuntimeError("ShmMessageBus create_reader failed, empty callback");
        }
        declare_channel(descriptor);
        maybe_register_reader(descriptor.name);
        const auto id = next_reader_id_.fetch_add(1, std::memory_order_relaxed);
        std::shared_ptr<ShmChannel> channel;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& channel_state = channels_[descriptor.name];
            channel_state.readers.emplace(id, callback);
            channel = channel_state.channel;
        }
        return std::make_shared<ShmReader>(this->shared_from_this(), descriptor.name, id, std::move(channel));
    }

    auto create_delivery_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<DeliveryWriter> override {
        declare_channel(descriptor);
        throw star::excep::RuntimeError("ShmMessageBus create_delivery_writer failed, delivery semantics are not supported for SHM stream channel: ", descriptor.name);
    }

    auto dump_stats_lines() const -> std::vector<std::string> override {
        auto out = std::vector<std::string>{};
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [channel_name, channel] : channels_) {
            if (channel.stats.write_count == 0 && channel.stats.deliver_count == 0) {
                continue;
            }
            out.emplace_back(format_stats_line("shm", channel_name, channel.stats));
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

  private:
    struct ChannelState {
        ChannelDescriptor descriptor;
        std::shared_ptr<ShmChannel> channel;
        std::unordered_map<uint64_t, Reader::Callback> readers;
        std::unordered_map<uint64_t, std::deque<MessageView>> reader_history;
        ChannelStats stats;
    };

    auto write_internal(const std::string& channel_name, const MessageView& message, const MessageSerializer* serializer) -> WriteResult {
        std::shared_ptr<ShmChannel> channel;
        uint64_t payload_size = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                auto out = WriteResult{};
                out.status = WriteStatus::CHANNEL_UNKNOWN;
                out.shm_required = true;
                return out;
            }
            channel = it->second.channel;
            payload_size = estimate_payload_size(message, serializer);
        }
        if (not channel) {
            auto out = WriteResult{};
            out.status = WriteStatus::CHANNEL_NOT_READY;
            out.shm_required = true;
            return out;
        }

        if (serializer == nullptr) {
            auto out = WriteResult{};
            out.status = WriteStatus::SERIALIZER_REQUIRED;
            out.shm_required = true;
            return out;
        }

        try {
            if (serializer->encode_into) {
                auto loaned = channel->loan(channel->slot_size());
                const auto size = serializer->encode_into(loaned.data(), loaned.capacity);
                if (size == 0) {
                    auto out = WriteResult{};
                    out.status = WriteStatus::ENCODE_FAILED;
                    out.shm_required = true;
                    return out;
                }
                loaned.commit(static_cast<uint32_t>(size));
                auto out = WriteResult{};
                out.status = WriteStatus::OK;
                out.shm_ok = true;
                out.shm_required = true;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = channels_.find(channel_name);
                    if (it != channels_.end()) {
                        it->second.stats.record_write(payload_size);
                    }
                }
                return out;
            }
            if (serializer->encode_binary) {
                const auto bytes = serializer->encode_binary();
                if (bytes.empty()) {
                    auto out = WriteResult{};
                    out.status = WriteStatus::ENCODE_FAILED;
                    out.shm_required = true;
                    return out;
                }
                channel->write(bytes.data(), static_cast<uint32_t>(bytes.size()));
                auto out = WriteResult{};
                out.status = WriteStatus::OK;
                out.shm_ok = true;
                out.shm_required = true;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    auto it = channels_.find(channel_name);
                    if (it != channels_.end()) {
                        it->second.stats.record_write(payload_size);
                    }
                }
                return out;
            }
            auto out = WriteResult{};
            out.status = WriteStatus::ENCODE_FAILED;
            out.shm_required = true;
            return out;
        } catch (const std::exception& e) {
            auto out = WriteResult{};
            out.status = WriteStatus::EXCEPTION;
            out.shm_required = true;
            out.error_message = e.what();
            return out;
        } catch (...) {
            auto out = WriteResult{};
            out.status = WriteStatus::EXCEPTION;
            out.shm_required = true;
            return out;
        }
    }

    void deliver_to_reader(const std::string& channel_name, uint64_t reader_id, uint32_t slot, uint32_t size, uint64_t seq) {
        Reader::Callback cb;
        MessageView msg;
        bool should_deliver = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                return;
            }
            auto cb_it = it->second.readers.find(reader_id);
            if (cb_it == it->second.readers.end()) {
                return;
            }
            cb = cb_it->second;

            const auto& options = it->second.descriptor.options;
            build_shm_message_view(channel_name, slot, size, seq, it, msg);
            if (options.depth > 0) {
                auto& history = it->second.reader_history[reader_id];
                auto result = apply_qos(history, msg, options, it->second.stats);
                if (!result.should_deliver) {
                    return;
                }
            }
            it->second.stats.record_deliver(1);
            should_deliver = true;
        }
        if (should_deliver) {
            cb(msg);
        }
    }

    void build_shm_message_view(const std::string& channel_name, uint32_t slot, uint32_t size, uint64_t seq, std::unordered_map<std::string, ChannelState>::iterator it, MessageView& msg) {
        msg.schema_type_id = it->second.descriptor.schema_type_id;
        msg.correlation_id = CorrelationId{};
        auto channel = it->second.channel;
        if (not channel) {
            return;
        }
        auto view = std::make_shared<ShmSlotView>();
        view->mapping = channel->mapping();
        view->data = channel->payload_ptr(slot);
        view->size = size;
        view->channel_name = channel_name;
        view->schema_type_id = it->second.descriptor.schema_type_id;
        // 代际信息取自 read() 返回的 seq，而非此刻的 current_seq()：
        // 只有前者才与 slot/size 同源，读侧据此复核该槽是否已被覆写
        view->seq_addr = channel->seq_addr();
        view->observed_seq = seq;
        view->slot_count = channel->slot_count();
        msg.payload = star::base::TypedConstPtr::from(std::move(view));
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

    void record_drop(const std::string& channel_name, uint64_t dropped) {
        if (dropped == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return;
        }
        it->second.stats.record_drop(dropped);
    }

    void record_latency(const std::string& channel_name, uint64_t write_ns) {
        if (write_ns == 0) {
            return;
        }
        const auto now_ns = ShmChannel::now_steady_ns();
        if (now_ns < write_ns) {
            return;
        }
        const auto latency_us = (now_ns - write_ns) / 1000;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return;
        }
        it->second.stats.record_latency(latency_us);
    }

    static auto resolve_capacity() -> uint32_t { return kShmCapacity; }

    static auto make_shm_name(const std::string& channel) -> std::string {
        const auto h = std::hash<std::string>{}(channel);
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "/starletto_%016lx", static_cast<unsigned long>(h));
        return std::string(buf);
    }

    static auto estimate_payload_size(const MessageView& message, const MessageSerializer* serializer) -> uint64_t {
        if (serializer != nullptr) {
            if (serializer->payload_size_hint > 0) {
                return serializer->payload_size_hint;
            }
        }
        if (const auto slot = message.payload.as<ShmSlotView>()) {
            return slot->size;
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
    std::atomic<uint64_t> next_reader_id_{1};
};

}  // namespace star::transport

#endif  // __SHM_MESSAGE_BUS_H__

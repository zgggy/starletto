#ifndef __SHM_MESSAGE_BUS_H__
#define __SHM_MESSAGE_BUS_H__

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
#include "basement/transport/shm/shm_channel.h"
#include "basement/transport/shm/shm_views.h"
#include "libraries/common/schema/schema_value.h"

namespace star::transport {

class ShmMessageBus final : public MessageBus, public std::enable_shared_from_this<ShmMessageBus> {
    class ShmWriter final : public Writer {
      public:
        ShmWriter(std::shared_ptr<ShmMessageBus> bus, std::string channel_name) : bus_(std::move(bus)), channel_name_(std::move(channel_name)) {}

        void write(const MessageView& message) override {
            auto bus = bus_.lock();
            if (not bus) {
                return;
            }
            bus->publish_internal(channel_name_, message);
        }

      private:
        std::weak_ptr<ShmMessageBus> bus_;
        std::string channel_name_;
    };

    class ShmReader final : public Reader {
      public:
        ShmReader(std::shared_ptr<ShmMessageBus> bus, std::string channel_name, uint64_t id, std::shared_ptr<ShmChannel> channel)
            : bus_(std::move(bus)), channel_name_(std::move(channel_name)), id_(id), channel_(std::move(channel)) {
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

      private:
        void poll_loop() {
            if (not channel_) {
                return;
            }
            const auto wait_ms = resolve_wait_ms();
            uint64_t last_seq = channel_->current_seq();
            while (not stop_.load(std::memory_order_relaxed)) {
                const auto r = channel_->read(&last_seq);
                if (r.has_value()) {
                    auto bus = bus_.lock();
                    if (bus) {
                        if (r->dropped > 0) {
                            bus->record_drop(channel_name_, r->dropped);
                        }
                        bus->record_latency(channel_name_, r->publish_ns);
                        bus->deliver_shm_view(channel_name_, r->slot, r->size);
                    }
                    continue;
                }
                channel_->wait_for_update(last_seq, wait_ms);
            }
        }

        static auto resolve_wait_ms() -> int64_t {
            if (const auto* v = std::getenv("STAR_SHM_WAIT_MS"); v != nullptr && std::string(v).size() > 0) {
                try {
                    return std::max<int64_t>(1, std::stoll(v));
                } catch (...) {
                }
            }
            return 100;
        }

        std::weak_ptr<ShmMessageBus> bus_;
        std::string channel_name_;
        uint64_t id_{0};
        std::shared_ptr<ShmChannel> channel_;
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
        const auto id = next_sub_id_.fetch_add(1, std::memory_order_relaxed);
        std::shared_ptr<ShmChannel> channel;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& channel_state = channels_[descriptor.name];
            channel_state.readers.emplace(id, callback);
            channel = channel_state.channel;
        }
        return std::make_shared<ShmReader>(this->shared_from_this(), descriptor.name, id, std::move(channel));
    }

    auto create_ack_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<AckWriter> override {
        (void)descriptor;
        return nullptr;
    }

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
            out.emplace_back("transport.shm channel=" + channel_name + " publish=" + std::to_string(channel.stats.publish_count) + " deliver=" + std::to_string(channel.stats.deliver_count) +
                             " dropped=" + std::to_string(channel.stats.dropped_count) + " payload_bytes_total=" + std::to_string(channel.stats.payload_bytes_total) +
                             " payload_bytes_last=" + std::to_string(channel.stats.payload_bytes_last) + " latency_avg_us=" + std::to_string(compute_avg_latency_us(channel.stats)) +
                             " latency_max_us=" + std::to_string(channel.stats.latency_max_us) + " latency_p99_us=" + std::to_string(p99) + " latency_p999_us=" + std::to_string(p999) +
                             " max_queue_depth=" + std::to_string(channel.stats.max_queue_depth) + " avg_hz=" + format_rate(hz));
        }
        return out;
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

    static auto compute_percentile(const std::vector<uint64_t>& samples, double p) -> uint64_t {
        if (samples.empty()) {
            return 0;
        }
        auto sorted = samples;
        std::sort(sorted.begin(), sorted.end());
        const auto idx = static_cast<size_t>(std::ceil(p * sorted.size())) - 1;
        return sorted[std::min(idx, sorted.size() - 1)];
    }

  private:
    struct ChannelState {
        ChannelDescriptor descriptor;
        std::shared_ptr<ShmChannel> channel;
        std::unordered_map<uint64_t, Reader::Callback> readers;
        std::unordered_map<uint64_t, std::deque<MessageView>> reader_history;
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
        } stats;
    };

    void publish_internal(const std::string& channel_name, const MessageView& message) {
        std::shared_ptr<ShmChannel> channel;
        uint64_t payload_size = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                return;
            }
            channel = it->second.channel;
            payload_size = estimate_payload_size(message);
            auto& stats = it->second.stats;
            const auto now = std::chrono::steady_clock::now();
            if (stats.publish_count == 0) {
                stats.first_publish_tp = now;
            }
            stats.last_publish_tp = now;
            stats.publish_count += 1;
            stats.payload_bytes_total += payload_size;
            stats.payload_bytes_last = payload_size;
        }
        if (not channel) {
            return;
        }

        if (message.encode_binary) {
            const auto bytes = message.encode_binary();
            channel->write(bytes.data(), static_cast<uint32_t>(bytes.size()));
        } else if (const auto conf = message.data.as<star::schema::SchemaValue>()) {
            auto loaned = channel->loan(channel->slot_size());
            const auto size = star::schema::dump_schema_text_into(*conf, reinterpret_cast<char*>(loaned.data()), static_cast<size_t>(loaned.capacity));
            loaned.commit(static_cast<uint32_t>(size));
        } else if (const auto view = message.data.as<ShmTextView>()) {
            channel->write(view->data, static_cast<uint32_t>(view->size));
        } else {
            throw star::excep::RuntimeError("ShmMessageBus publish failed, unsupported message payload for channel: ", channel_name);
        }
    }

    void deliver_shm_view(const std::string& channel_name, uint32_t slot, uint32_t size) {
        std::vector<std::pair<uint64_t, Reader::Callback>> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                return;
            }
            const auto& options = it->second.descriptor.options;
            for (const auto& [reader_id, cb] : it->second.readers) {
                if (options.depth > 0) {
                    auto& history = it->second.reader_history[reader_id];
                    MessageView msg_snapshot;
                    msg_snapshot.schema_type_id = it->second.descriptor.schema_type_id;
                    if (options.drop_policy == DropPolicy::KEEP_LAST) {
                        build_shm_message_view(channel_name, slot, size, it, msg_snapshot);
                        history.push_back(msg_snapshot);
                        while (history.size() > options.depth) {
                            history.pop_front();
                            it->second.stats.dropped_count += 1;
                        }
                    } else {
                        if (history.size() >= options.depth) {
                            it->second.stats.dropped_count += 1;
                            continue;
                        }
                        build_shm_message_view(channel_name, slot, size, it, msg_snapshot);
                        history.push_back(msg_snapshot);
                    }
                    it->second.stats.max_queue_depth = std::max(it->second.stats.max_queue_depth, history.size());
                }
                callbacks.emplace_back(reader_id, cb);
            }
            it->second.stats.deliver_count += callbacks.size();
        }
        for (const auto& [reader_id, cb] : callbacks) {
            MessageView msg;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = channels_.find(channel_name);
                if (it == channels_.end()) {
                    continue;
                }
                const auto& options = it->second.descriptor.options;
                if (options.depth > 0) {
                    auto hist_it = it->second.reader_history.find(reader_id);
                    if (hist_it != it->second.reader_history.end() && not hist_it->second.empty()) {
                        msg = hist_it->second.back();
                    } else {
                        continue;
                    }
                } else {
                    build_shm_message_view(channel_name, slot, size, it, msg);
                }
            }
            cb(msg);
        }
    }

    void build_shm_message_view(const std::string& channel_name, uint32_t slot, uint32_t size, std::unordered_map<std::string, ChannelState>::iterator it, MessageView& msg) {
        msg.schema_type_id = it->second.descriptor.schema_type_id;
        auto channel = it->second.channel;
        if (not channel) {
            return;
        }
        if (it->second.descriptor.type_name == "star::schema::SchemaValue") {
            auto view = std::make_shared<ShmTextView>();
            view->mapping = channel->mapping();
            view->data = reinterpret_cast<const char*>(channel->payload_ptr(slot));
            view->size = size;
            view->channel_name = channel_name;
            view->schema_type_id = it->second.descriptor.schema_type_id;
            msg.data = star::base::TypedConstPtr::from(std::move(view));
        } else {
            auto view = std::make_shared<ShmBinaryView>();
            view->mapping = channel->mapping();
            view->data = channel->payload_ptr(slot);
            view->size = size;
            view->channel_name = channel_name;
            view->schema_type_id = it->second.descriptor.schema_type_id;
            msg.data = star::base::TypedConstPtr::from(std::move(view));
        }
    }

    void erase_reader(const std::string& channel_name, uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return;
        }
        it->second.readers.erase(id);
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
        it->second.stats.dropped_count += dropped;
    }

    void record_latency(const std::string& channel_name, uint64_t publish_ns) {
        if (publish_ns == 0) {
            return;
        }
        const auto now_ns = ShmChannel::now_steady_ns();
        if (now_ns < publish_ns) {
            return;
        }
        const auto latency_us = (now_ns - publish_ns) / 1000;
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it == channels_.end()) {
            return;
        }
        it->second.stats.latency_total_us += latency_us;
        it->second.stats.latency_max_us = std::max(it->second.stats.latency_max_us, latency_us);
        if (it->second.stats.latency_samples.size() < 1000) {
            it->second.stats.latency_samples.push_back(latency_us);
        }
    }

    static auto resolve_capacity() -> uint32_t { return kShmCapacity; }

    static auto make_shm_name(const std::string& channel) -> std::string {
        const auto h = std::hash<std::string>{}(channel);
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "/starletto_%016lx", static_cast<unsigned long>(h));
        return std::string(buf);
    }

    static auto estimate_payload_size(const MessageView& message) -> uint64_t {
        if (message.encode_binary) {
            return message.encode_binary().size();
        }
        if (const auto conf = message.data.as<star::schema::SchemaValue>()) {
            return star::schema::dump_schema_text(*conf).size();
        }
        if (const auto view = message.data.as<ShmTextView>()) {
            return view->size;
        }
        if (const auto view = message.data.as<ShmBinaryView>()) {
            return view->size;
        }
        return 0;
    }

    static auto compute_hz(const ChannelState::ChannelStats& stats) -> double {
        if (stats.publish_count <= 1) {
            return static_cast<double>(stats.publish_count);
        }
        const auto seconds = std::chrono::duration<double>(stats.last_publish_tp - stats.first_publish_tp).count();
        if (seconds <= 0.0) {
            return static_cast<double>(stats.publish_count);
        }
        return static_cast<double>(stats.publish_count - 1) / seconds;
    }

    static auto compute_avg_latency_us(const ChannelState::ChannelStats& stats) -> uint64_t {
        if (stats.deliver_count == 0) {
            return 0;
        }
        return stats.latency_total_us / stats.deliver_count;
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
};

}  // namespace star::transport

#endif  // __SHM_MESSAGE_BUS_H__

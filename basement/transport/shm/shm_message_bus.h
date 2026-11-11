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

    // 只持有 (channel, reader_id) 注册关系；轮询线程属于通道，不是每个 reader 一个。
    // 8 个相机 × 3 个消费者时，线程数从 24 降到 8。
    class ShmReader final : public Reader {
      public:
        ShmReader(std::shared_ptr<ShmMessageBus> bus, std::string channel_name, uint64_t id)
            : bus_(std::move(bus)), channel_name_(std::move(channel_name)), id_(id) {}

        ~ShmReader() override {
            auto bus = bus_.lock();
            if (bus) {
                bus->remove_reader(channel_name_, id_);
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
        std::weak_ptr<ShmMessageBus> bus_;
        std::string channel_name_;
        uint64_t id_{0};
    };

  public:
    explicit ShmMessageBus(std::shared_ptr<ChannelRegistry> channel_registry = nullptr) : channel_registry_(std::move(channel_registry)) {}
    ~ShmMessageBus() override {
        auto pollers = std::vector<std::shared_ptr<ChannelPoller>>{};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& [_, state] : channels_) {
                if (state.poller) {
                    pollers.push_back(std::exchange(state.poller, nullptr));
                }
            }
        }
        for (auto& poller : pollers) {
            poller->stop.store(true, std::memory_order_relaxed);
            if (poller->channel) {
                poller->channel->wake_waiters();
            }
            if (poller->thread.joinable()) {
                poller->thread.join();
            }
        }
    }

    void declare_channel(const ChannelDescriptor& descriptor) override {
        if (descriptor.name.empty()) {
            throw star::excep::RuntimeError("ShmMessageBus declare_channel failed, empty channel name");
        }
        validate_qos_options(descriptor.options, descriptor.name);
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
            } else if (not descriptor.schema_type_id.empty() and
                       channel.descriptor.schema_type_id.name != descriptor.schema_type_id.name) {
                throw star::excep::RuntimeError("ShmMessageBus declare_channel failed, schema name mismatch for channel: ", descriptor.name,
                                                ", existing=", channel.descriptor.schema_type_id.name,
                                                ", new=", descriptor.schema_type_id.name);
            }
            // 解码策略声明在接收端：先声明的是 writer（默认策略），reader 的显式策略要覆盖上来
            if (descriptor.options.decode_policy != star::schema::DecodePolicy{} and
                channel.descriptor.options.decode_policy == star::schema::DecodePolicy{}) {
                channel.descriptor.options.decode_policy = descriptor.options.decode_policy;
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
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto& channel_state = channels_[descriptor.name];
            channel_state.readers.emplace(id, std::move(callback));
            ensure_poller_locked(descriptor.name, channel_state);
        }
        return std::make_shared<ShmReader>(this->shared_from_this(), descriptor.name, id);
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

    // 当前活跃的通道轮询线程数（每通道至多一个）
    auto poller_count() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto& [_, state] : channels_) {
            if (state.poller) {
                ++count;
            }
        }
        return count;
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
    // 每个通道一个轮询线程，多个 reader 共享：读取一次、按各 reader 的 QoS 分发。
    // 之前每个 reader 一个线程（每线程各自 attach 同一段并各自读），
    // 8 相机 × 3 消费者就是 24 个线程轮询 8 个段。
    struct ChannelPoller {
        std::shared_ptr<ShmChannel> channel;
        // 基线必须在启动线程之前捕获：若放到线程里取，订阅建立后、线程跑起来前
        // 发布的消息会被误判为「已见过」而丢失
        uint64_t baseline_seq{0};
        std::atomic<bool> stop{false};
        std::thread thread;

        ~ChannelPoller() {
            stop.store(true, std::memory_order_relaxed);
            if (channel) {
                channel->wake_waiters();
            }
            if (thread.joinable()) {
                thread.join();
            }
        }
    };

    struct ChannelState {
        ChannelDescriptor descriptor;
        std::shared_ptr<ShmChannel> channel;
        std::unordered_map<uint64_t, Reader::Callback> readers;
        std::unordered_map<uint64_t, std::deque<MessageView>> reader_history;
        ChannelStats stats;
        std::shared_ptr<ChannelPoller> poller;
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
            // 类型一致性 + 首写采纳（与 native 同语义）
            // 通道身份只强制 name 一致：同名不同 version/hash 是跨版本通信，
            // 由接收端的 decode_policy 决定接受还是拒绝（决策二/三）
            if (not it->second.descriptor.schema_type_id.empty() and not message.schema_type_id.empty() and
                it->second.descriptor.schema_type_id.name != message.schema_type_id.name) {
                throw star::excep::RuntimeError("ShmMessageBus write failed, schema name mismatch for channel: ", channel_name, ", existing=",
                                                it->second.descriptor.schema_type_id.name, ", message=", message.schema_type_id.name);
            }
            if (it->second.descriptor.schema_type_id.empty() and not message.schema_type_id.empty()) {
                it->second.descriptor.schema_type_id = message.schema_type_id;
            }
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
            // 优先二进制 TLV：它是唯一支持跨版本演进（未知字段保留、缺失补默认）的格式。
            // 固定布局没有字段 id，只能同版本通信；仅当消息没有二进制编码器时才用它。
            if (serializer->encode_binary) {
                const auto bytes = serializer->encode_binary();
                if (bytes.empty()) {
                    auto out = WriteResult{};
                    out.status = WriteStatus::ENCODE_FAILED;
                    out.shm_required = true;
                    return out;
                }
                // 单帧超限是「这条消息太大」而非通道故障：显式判为 ENCODE_FAILED，
                // 不能让 channel->write 抛异常被记成 EXCEPTION（那会触发通道降级）
                if (bytes.size() > channel->slot_size()) {
                    auto out = WriteResult{};
                    out.status = WriteStatus::ENCODE_FAILED;
                    out.shm_required = true;
                    out.error_message = "payload exceeds slot size: " + std::to_string(bytes.size()) + " > " + std::to_string(channel->slot_size());
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

    // 一次读取分发给该通道的所有 reader；QoS 历史按 reader 各自维护。
    void deliver_to_all(const std::string& channel_name, uint32_t slot, uint32_t size, uint32_t gen) {
        auto callbacks = std::vector<std::pair<uint64_t, Reader::Callback>>{};
        MessageView message;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end() or it->second.readers.empty()) {
                return;
            }
            build_shm_message_view(channel_name, slot, size, gen, it, message);
            const auto& options = it->second.descriptor.options;
            callbacks.reserve(it->second.readers.size());
            for (const auto& [reader_id, callback] : it->second.readers) {
                if (options.depth > 0) {
                    auto& history = it->second.reader_history[reader_id];
                    auto result = apply_qos(history, message, options, it->second.stats);
                    if (not result.should_deliver) {
                        continue;
                    }
                }
                callbacks.emplace_back(reader_id, callback);
            }
            it->second.stats.record_deliver(callbacks.size());
        }

        for (const auto& [_, callback] : callbacks) {
            // 订阅回调运行在通道轮询线程上：异常若逃逸会 terminate 整个进程
            try {
                callback(message);
            } catch (const std::exception& e) {
                record_callback_failure(channel_name);
                logE << "ShmMessageBus: reader callback threw: channel=" << channel_name << ", err=" << e.what();
            } catch (...) {
                record_callback_failure(channel_name);
                logE << "ShmMessageBus: reader callback threw unknown exception: channel=" << channel_name;
            }
        }
    }

    void poll_channel(const std::string& channel_name, const std::shared_ptr<ChannelPoller>& poller) {
        const auto wait_ms = resolve_wait_ms();
        uint64_t last_seq = poller->baseline_seq;
        while (not poller->stop.load(std::memory_order_relaxed)) {
            if (not poller->channel) {
                return;
            }
            const auto r = poller->channel->read(&last_seq);
            if (r.has_value()) {
                if (r->dropped > 0) {
                    record_drop(channel_name, r->dropped);
                }
                record_latency(channel_name, r->write_ns);
                deliver_to_all(channel_name, r->slot, r->size, r->gen);
                continue;
            }
            poller->channel->wait_for_update(last_seq, wait_ms);
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

    void record_callback_failure(const std::string& channel_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = channels_.find(channel_name);
        if (it != channels_.end()) {
            it->second.stats.record_callback_failure();
        }
    }

    void build_shm_message_view(const std::string& channel_name, uint32_t slot, uint32_t size, uint32_t gen, std::unordered_map<std::string, ChannelState>::iterator it, MessageView& msg) {
        msg.schema_type_id = it->second.descriptor.schema_type_id;
        msg.decode_policy = it->second.descriptor.options.decode_policy;
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
        // 代际必须取自 read() 返回的 gen，而非此刻的实时值：
        // 只有前者才与 slot/size 同源，读侧据此在解码前后复核该槽
        view->gen_addr = channel->gen_addr(slot);
        view->observed_gen = gen;
        msg.payload = star::base::TypedConstPtr::from(std::move(view));
    }

    // reader 析构必须等到在途回调结束：轮询线程可能正持有一份 callback 拷贝。
    // 做法是停掉该通道的 poller 并 join（锁外），若还有其它 reader 再重启。
    // 重启的基线取当前 seq，teardown 窗口内到达的消息会跳过——reader 销毁不是热路径。
    void remove_reader(const std::string& channel_name, uint64_t id) {
        std::shared_ptr<ChannelPoller> poller;
        bool need_restart = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it == channels_.end()) {
                return;
            }
            it->second.readers.erase(id);
            it->second.reader_history.erase(id);
            if (it->second.poller) {
                poller = std::exchange(it->second.poller, nullptr);
                need_restart = not it->second.readers.empty();
            }
        }
        if (poller) {
            // join 必须在锁外：轮询线程可能正等待 mutex 派发回调
            poller->stop.store(true, std::memory_order_relaxed);
            if (poller->channel) {
                poller->channel->wake_waiters();
            }
            if (poller->thread.joinable()) {
                poller->thread.join();
            }
        }
        if (need_restart) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = channels_.find(channel_name);
            if (it != channels_.end()) {
                ensure_poller_locked(channel_name, it->second);
            }
        }
    }

    void ensure_poller_locked(const std::string& channel_name, ChannelState& state) {
        if (state.poller or state.readers.empty() or not state.channel) {
            return;
        }
        auto poller = std::make_shared<ChannelPoller>();
        poller->channel = state.channel;
        poller->baseline_seq = state.channel->current_seq();
        poller->thread = std::thread([this, channel_name, poller]() { poll_channel(channel_name, poller); });
        state.poller = std::move(poller);
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
        // 段名含版本号：header 布局变更后，旧段名不再被命中（避免按错误布局解析）
        std::snprintf(buf, sizeof(buf), "/star_v%02u_%016lx", kShmVersion, static_cast<unsigned long>(h));
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

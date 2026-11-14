#ifndef __SHM_ROUTED_MESSAGE_BUS_H__
#define __SHM_ROUTED_MESSAGE_BUS_H__

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/native/native_message_bus.h"
#include "basement/transport/shm/shm_message_bus.h"

namespace star::transport {

class ShmRoutedMessageBus final : public MessageBus, public RecordableBus, public std::enable_shared_from_this<ShmRoutedMessageBus> {
  public:
    explicit ShmRoutedMessageBus(std::shared_ptr<ChannelRegistry> channel_registry = nullptr)
        : channel_registry_(channel_registry ? std::move(channel_registry) : std::make_shared<ChannelRegistry>()),
          inproc_(std::make_shared<NativeMessageBus>(channel_registry_)) {
        try {
            shm_ = std::make_shared<ShmMessageBus>(channel_registry_);
        } catch (const star::excep::RuntimeError& e) {
            logW << "ShmRoutedMessageBus: SHM init failed, falling back to inproc-only: " << e.what();
            shm_available_ = false;
        }
    }

    ~ShmRoutedMessageBus() override = default;

    auto shm_available() const -> bool { return shm_available_; }

    void configure_route_plan(std::string shm_pub_channels_csv, std::string shm_sub_channels_csv) {
        channel_registry_->configure_channel_sets(shm_pub_channels_csv, shm_sub_channels_csv);
    }

    void configure_route_registry(std::string local_group, std::string route_plan) {
        channel_registry_->configure_local_group(std::move(local_group));
        channel_registry_->configure_route_plan(route_plan);
    }

    // 系统通道（统计）由本进程发布、其它进程订阅：即使不在业务路由计划里也必须走 shm
    void register_system_publisher(const std::string& channel) { channel_registry_->register_system_publisher(channel); }

    // 系统读写通道（服务 request/response）：任何进程都可能读或写，始终走 shm
    void register_system_channel(const std::string& channel) { channel_registry_->register_system_channel(channel); }

    // 录制委托给内部 NativeMessageBus：routed 写路径本来就先走 inproc，
    // 因此录制发生在 inproc 侧即可覆盖全部发布；此前未继承 RecordableBus，
    // 导致 record:true 在路由模式下静默无效
    void enable_recorder(const std::string& file_path) override { inproc_->enable_recorder(file_path); }
    void enable_record_channel(const std::string& channel) override { inproc_->enable_record_channel(channel); }
    void disable_recorder() override { inproc_->disable_recorder(); }
    void disable_record_channel(const std::string& channel) override { inproc_->disable_record_channel(channel); }

    void declare_channel(const ChannelDescriptor& descriptor) override {
        inproc_->declare_channel(descriptor);
        if (not shm_available_) {
            return;
        }
        warn_if_missing_from_route_plan(descriptor.name);
        if (should_use_shm_for_channel(descriptor.name)) {
            try {
                shm_->declare_channel(descriptor);
            } catch (const star::excep::RuntimeError& e) {
                logW << "ShmRoutedMessageBus: declare_channel SHM failed for " << descriptor.name << ", degrading: " << e.what();
                degrade_channel(descriptor.name);
            }
        }
    }

    auto create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> override {
        declare_channel(descriptor);
        auto writer_inproc = inproc_->create_writer(descriptor);
        std::shared_ptr<Writer> writer_shm;
        if (shm_available_ && not is_channel_degraded(descriptor.name)) {
            const auto mode = channel_registry_->shm_route_mode(descriptor.name);
            const bool should_create_shm_writer = mode == ChannelRegistry::ShmRouteMode::SHM_WRITE || mode == ChannelRegistry::ShmRouteMode::SHM_READ_WRITE;
            if (!should_create_shm_writer) {
                return std::make_shared<RoutedWriter>(this->shared_from_this(), descriptor.name, std::move(writer_inproc), nullptr);
            }
            try {
                writer_shm = shm_->create_writer(descriptor);
            } catch (const star::excep::RuntimeError& e) {
                logW << "ShmRoutedMessageBus: create_writer SHM failed for " << descriptor.name << ", degrading: " << e.what();
                degrade_channel(descriptor.name);
            }
        }
        return std::make_shared<RoutedWriter>(this->shared_from_this(), descriptor.name, std::move(writer_inproc), std::move(writer_shm));
    }

    auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> override {
        if (not callback) {
            throw star::excep::RuntimeError("ShmRoutedMessageBus create_reader failed, empty callback");
        }
        declare_channel(descriptor);

        std::shared_ptr<Reader> reader_inproc;
        std::shared_ptr<Reader> reader_shm;

        if (not shm_available_ || is_channel_degraded(descriptor.name)) {
            reader_inproc = inproc_->create_reader(descriptor, callback);
            return std::make_shared<RoutedReader>(std::move(reader_inproc), nullptr);
        }

        const auto mode = channel_registry_->shm_route_mode(descriptor.name);
        // 系统服务通道必须挂 shm reader：本进程读不到其它进程写入的消息。
        // 普通 READ_WRITE（pub/sub 集合重叠）保持原来的 inproc 读：本地写入
        // 不经过 4 槽 shm 环，不会被成批覆盖。
        const bool system_channel = channel_registry_->is_system_channel(descriptor.name);
        if (mode == ChannelRegistry::ShmRouteMode::SHM_READ or
            (mode == ChannelRegistry::ShmRouteMode::SHM_READ_WRITE and system_channel)) {
            try {
                reader_shm = shm_->create_reader(descriptor, callback);
            } catch (const star::excep::RuntimeError& e) {
                logW << "ShmRoutedMessageBus: create_reader SHM failed for " << descriptor.name << ", degrading: " << e.what();
                degrade_channel(descriptor.name);
                reader_inproc = inproc_->create_reader(descriptor, callback);
            }
            return std::make_shared<RoutedReader>(std::move(reader_inproc), std::move(reader_shm));
        }

        reader_inproc = inproc_->create_reader(descriptor, callback);
        return std::make_shared<RoutedReader>(std::move(reader_inproc), nullptr);
    }

    auto create_delivery_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<DeliveryWriter> override {
        declare_channel(descriptor);
        return inproc_->create_delivery_writer(descriptor);
    }

    auto dump_stats_lines() const -> std::vector<std::string> override {
        auto out = inproc_->dump_stats_lines();
        if (shm_available_) {
            auto shm_lines = shm_->dump_stats_lines();
            out.insert(out.end(), shm_lines.begin(), shm_lines.end());
        }
        return out;
    }

    auto channel_stats(const std::string& channel_name) const -> std::optional<ChannelStatsSnapshot> override {
        auto inproc_stats = inproc_->channel_stats(channel_name);
        if (not shm_available_ || is_channel_degraded(channel_name)) {
            return inproc_stats;
        }
        auto shm_stats = shm_->channel_stats(channel_name);
        if (inproc_stats && shm_stats) {
            auto merged = *inproc_stats;
            merged.write_count += shm_stats->write_count;
            merged.deliver_count += shm_stats->deliver_count;
            merged.dropped_count += shm_stats->dropped_count;
            merged.payload_bytes_total += shm_stats->payload_bytes_total;
            return merged;
        }
        return inproc_stats ? inproc_stats : shm_stats;
    }

    auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot> override {
        auto out = inproc_->all_channel_stats();
        if (shm_available_) {
            auto shm_stats = shm_->all_channel_stats();
            out.insert(out.end(), shm_stats.begin(), shm_stats.end());
        }
        return out;
    }

    auto declared_channels() const -> std::vector<std::string> override { return inproc_->declared_channels(); }

  private:
    class RoutedWriter final : public Writer {
      public:
        RoutedWriter(std::shared_ptr<ShmRoutedMessageBus> bus, std::string channel_name, std::shared_ptr<Writer> inproc, std::shared_ptr<Writer> shm)
            : bus_(std::move(bus)), channel_name_(std::move(channel_name)), inproc_(std::move(inproc)), shm_(std::move(shm)) {}

        auto write(const MessageView& message) -> WriteResult override {
            auto out = WriteResult{};
            if (inproc_) {
                const auto r = inproc_->write(message);
                out.inproc_ok = r.inproc_ok || r.ok();
                if (!r.ok()) {
                    out.status = r.status;
                    out.error_message = r.error_message;
                    return out;
                }
            }
            auto bus = bus_.lock();
            if (!bus) {
                out.status = WriteStatus::BUS_EXPIRED;
                return out;
            }
            const auto mode = bus->shm_route_mode(channel_name_);
            const bool should_write = mode == ChannelRegistry::ShmRouteMode::SHM_WRITE || mode == ChannelRegistry::ShmRouteMode::SHM_READ_WRITE;
            if (!should_write || bus->is_channel_degraded(channel_name_)) {
                out.status = WriteStatus::OK;
                return out;
            }
            out.shm_required = true;
            out.status = WriteStatus::SERIALIZER_REQUIRED;
            return out;
        }

        auto write_serialized(const SerializedMessage& message) -> WriteResult override {
            auto out = WriteResult{};
            if (inproc_) {
                const auto r = inproc_->write_serialized(message);
                out.inproc_ok = r.inproc_ok || r.ok();
                if (!r.ok()) {
                    out.status = r.status;
                    out.error_message = r.error_message;
                    return out;
                }
            }
            auto bus = bus_.lock();
            if (!bus) {
                out.status = WriteStatus::BUS_EXPIRED;
                return out;
            }
            const auto mode = bus->shm_route_mode(channel_name_);
            const bool should_write = mode == ChannelRegistry::ShmRouteMode::SHM_WRITE || mode == ChannelRegistry::ShmRouteMode::SHM_READ_WRITE;
            if (!should_write) {
                out.status = WriteStatus::OK;
                return out;
            }
            out.shm_required = true;
            if (bus->is_channel_degraded(channel_name_)) {
                out.status = WriteStatus::SHM_DEGRADED;
                return out;
            }
            if (!shm_) {
                out.status = WriteStatus::SHM_DISABLED;
                return out;
            }
            const auto r = shm_->write_serialized(message);
            out.shm_ok = r.shm_ok || r.ok();
            out.status = r.status;
            out.error_message = r.error_message;
            // ENCODE_FAILED 是「这一帧太大」的单帧问题，不是通道故障：
            // 降级整个通道会让后续正常大小的帧也永远发不出去
            if (r.status == WriteStatus::EXCEPTION) {
                bus->degrade_channel(channel_name_);
            }
            return out;
        }

      private:
        std::weak_ptr<ShmRoutedMessageBus> bus_;
        std::string channel_name_;
        std::shared_ptr<Writer> inproc_;
        std::shared_ptr<Writer> shm_;
    };

    class RoutedReader final : public Reader {
      public:
        RoutedReader(std::shared_ptr<Reader> inproc, std::shared_ptr<Reader> shm) : inproc_(std::move(inproc)), shm_(std::move(shm)) {}
        ~RoutedReader() override = default;

        auto peek_latest() const -> std::optional<MessageView> override {
            if (shm_) {
                auto result = shm_->peek_latest();
                if (result.has_value()) {
                    return result;
                }
            }
            if (inproc_) {
                return inproc_->peek_latest();
            }
            return std::nullopt;
        }

        auto has_data() const -> bool override {
            if (shm_ && shm_->has_data()) {
                return true;
            }
            return inproc_ && inproc_->has_data();
        }

      private:
        std::shared_ptr<Reader> inproc_;
        std::shared_ptr<Reader> shm_;
    };

    static constexpr std::chrono::milliseconds kDegradeTtl{5000};

    // 路由计划存在但缺了本通道：跨进程订阅者永远收不到数据，
    // 而写入仍返回 OK——这是最难排查的一类静默失效，必须在启动时喊出来
    void warn_if_missing_from_route_plan(const std::string& channel_name) {
        if (channel_registry_ == nullptr || not channel_registry_->has_route_plan()) {
            return;
        }
        if (channel_registry_->is_in_route_plan(channel_name) or channel_registry_->is_system_channel(channel_name)) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (not warned_channels_.insert(channel_name).second) {
                return;
            }
        }
        logW << "ShmRoutedMessageBus: channel not present in route plan, stays inproc-only; cross-process subscribers will NOT receive it. channel=" << channel_name;
    }

    void degrade_channel(const std::string& channel_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        degraded_channels_[channel_name] = std::chrono::steady_clock::now();
        logW << "ShmRoutedMessageBus: channel degraded to inproc-only for " << std::chrono::duration_cast<std::chrono::milliseconds>(kDegradeTtl).count()
             << "ms, channel=" << channel_name;
    }

    auto is_channel_degraded(const std::string& channel_name) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = degraded_channels_.find(channel_name);
        if (it == degraded_channels_.end()) {
            return false;
        }
        if (std::chrono::steady_clock::now() - it->second >= kDegradeTtl) {
            degraded_channels_.erase(it);  // 过期后自动恢复尝试
            return false;
        }
        return true;
    }

    auto shm_route_mode(const std::string& channel) const -> ChannelRegistry::ShmRouteMode {
        if (not shm_available_) {
            return ChannelRegistry::ShmRouteMode::INPROC_ONLY;
        }
        return channel_registry_->shm_route_mode(channel);
    }

    auto should_use_shm_for_channel(const std::string& channel) const -> bool {
        return shm_route_mode(channel) != ChannelRegistry::ShmRouteMode::INPROC_ONLY;
    }

    std::shared_ptr<ChannelRegistry> channel_registry_;
    std::shared_ptr<NativeMessageBus> inproc_;
    std::shared_ptr<ShmMessageBus> shm_;
    bool shm_available_{true};
    mutable std::mutex mutex_;
    // 降级带 TTL：SHM 故障可能是瞬时的（如对端重启），过期后自动重试，
    // 避免一次失败就让该通道永久失去跨进程能力
    mutable std::unordered_map<std::string, std::chrono::steady_clock::time_point> degraded_channels_;
    std::unordered_set<std::string> warned_channels_;
};

}  // namespace star::transport

#endif  // __SHM_ROUTED_MESSAGE_BUS_H__

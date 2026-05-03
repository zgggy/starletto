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

class ShmRoutedMessageBus final : public MessageBus, public std::enable_shared_from_this<ShmRoutedMessageBus> {
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

    void declare_channel(const ChannelDescriptor& descriptor) override {
        inproc_->declare_channel(descriptor);
        if (not shm_available_) {
            return;
        }
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
        if (mode == ChannelRegistry::ShmRouteMode::SHM_READ) {
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
            if (r.status == WriteStatus::EXCEPTION || r.status == WriteStatus::ENCODE_FAILED) {
                bus->degrade_channel(channel_name_);
                shm_.reset();
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

    void degrade_channel(const std::string& channel_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        degraded_channels_.insert(channel_name);
    }

    auto is_channel_degraded(const std::string& channel_name) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        return degraded_channels_.count(channel_name) > 0;
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
    std::unordered_set<std::string> degraded_channels_;
};

}  // namespace star::transport

#endif  // __SHM_ROUTED_MESSAGE_BUS_H__

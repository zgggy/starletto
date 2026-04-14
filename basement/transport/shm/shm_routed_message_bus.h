#ifndef __SHM_ROUTED_MESSAGE_BUS_H__
#define __SHM_ROUTED_MESSAGE_BUS_H__

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/native/native_message_bus.h"
#include "basement/transport/shm/shm_message_bus.h"

namespace star::transport {

class ShmRoutedMessageBus final : public MessageBus, public std::enable_shared_from_this<ShmRoutedMessageBus> {
  public:
    explicit ShmRoutedMessageBus(std::shared_ptr<ChannelRegistry> channel_registry = nullptr)
        : channel_registry_(std::move(channel_registry)), inproc_(std::make_shared<NativeMessageBus>(channel_registry_)), shm_(std::make_shared<ShmMessageBus>(channel_registry_)) {}

    ~ShmRoutedMessageBus() override = default;

    void configure_route_plan(std::string shm_pub_channels_csv, std::string shm_sub_channels_csv) {
        std::lock_guard<std::mutex> lock(mutex_);
        shm_pub_channels_ = parse_channel_set(shm_pub_channels_csv);
        shm_sub_channels_ = parse_channel_set(shm_sub_channels_csv);
    }

    void configure_route_registry(std::string local_group, std::string route_plan) {
        if (channel_registry_) {
            channel_registry_->configure_local_group(std::move(local_group));
            channel_registry_->configure_route_plan(route_plan);
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        local_group_ = std::move(local_group);
        route_registry_ = parse_route_plan(route_plan);
    }

    void declare_channel(const ChannelDescriptor& descriptor) override {
        inproc_->declare_channel(descriptor);
        if (should_use_shm_for_channel(descriptor.name)) {
            shm_->declare_channel(descriptor);
        }
    }

    auto create_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<Writer> override {
        declare_channel(descriptor);
        auto writer_inproc = inproc_->create_writer(descriptor);
        std::shared_ptr<Writer> writer_shm;
        if (should_publish_shm(descriptor.name)) {
            writer_shm = shm_->create_writer(descriptor);
        }
        return std::make_shared<RoutedWriter>(this->shared_from_this(), std::move(writer_inproc), std::move(writer_shm));
    }

    auto create_reader(const ChannelDescriptor& descriptor, Reader::Callback callback) -> std::shared_ptr<Reader> override {
        if (not callback) {
            throw star::excep::RuntimeError("ShmRoutedMessageBus create_reader failed, empty callback");
        }
        declare_channel(descriptor);
        auto reader_inproc = inproc_->create_reader(descriptor, callback);
        std::shared_ptr<Reader> reader_shm;
        if (should_subscribe_shm(descriptor.name)) {
            reader_shm = shm_->create_reader(descriptor, callback);
        }
        return std::make_shared<RoutedReader>(std::move(reader_inproc), std::move(reader_shm));
    }

    auto create_ack_writer(const ChannelDescriptor& descriptor) -> std::shared_ptr<AckWriter> override {
        declare_channel(descriptor);
        return inproc_->create_ack_writer(descriptor);
    }

    auto dump_stats_lines() const -> std::vector<std::string> override {
        auto out = inproc_->dump_stats_lines();
        auto shm_lines = shm_->dump_stats_lines();
        out.insert(out.end(), shm_lines.begin(), shm_lines.end());
        return out;
    }

    auto channel_stats(const std::string& channel_name) const -> std::optional<ChannelStatsSnapshot> override {
        auto inproc_stats = inproc_->channel_stats(channel_name);
        auto shm_stats = shm_->channel_stats(channel_name);
        if (inproc_stats && shm_stats) {
            auto merged = *inproc_stats;
            merged.publish_count += shm_stats->publish_count;
            merged.deliver_count += shm_stats->deliver_count;
            merged.dropped_count += shm_stats->dropped_count;
            merged.payload_bytes_total += shm_stats->payload_bytes_total;
            return merged;
        }
        return inproc_stats ? inproc_stats : shm_stats;
    }

    auto all_channel_stats() const -> std::vector<ChannelStatsSnapshot> override {
        auto out = inproc_->all_channel_stats();
        auto shm_stats = shm_->all_channel_stats();
        out.insert(out.end(), shm_stats.begin(), shm_stats.end());
        return out;
    }

    auto declared_channels() const -> std::vector<std::string> override { return inproc_->declared_channels(); }

  private:
    class RoutedWriter final : public Writer {
      public:
        RoutedWriter(std::shared_ptr<ShmRoutedMessageBus> bus, std::shared_ptr<Writer> inproc, std::shared_ptr<Writer> shm) : bus_(std::move(bus)), inproc_(std::move(inproc)), shm_(std::move(shm)) {}

        void write(const MessageView& message) override {
            if (inproc_) {
                inproc_->write(message);
            }
            if (shm_) {
                shm_->write(message);
            }
        }

      private:
        std::shared_ptr<ShmRoutedMessageBus> bus_;
        std::shared_ptr<Writer> inproc_;
        std::shared_ptr<Writer> shm_;
    };

    class RoutedReader final : public Reader {
      public:
        RoutedReader(std::shared_ptr<Reader> inproc, std::shared_ptr<Reader> shm) : inproc_(std::move(inproc)), shm_(std::move(shm)) {}
        ~RoutedReader() override = default;

      private:
        std::shared_ptr<Reader> inproc_;
        std::shared_ptr<Reader> shm_;
    };

    auto should_publish_shm(const std::string& channel) const -> bool {
        if (channel_registry_ && channel_registry_->should_publish_shm(channel)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (not local_group_.empty()) {
            const auto it = route_registry_.find(channel);
            if (it != route_registry_.end() && it->second.producer_group == local_group_) {
                for (const auto& consumer_group : it->second.consumer_groups) {
                    if (consumer_group != local_group_) {
                        return true;
                    }
                }
                return false;
            }
        }
        return shm_pub_channels_.find(channel) != shm_pub_channels_.end();
    }

    auto should_subscribe_shm(const std::string& channel) const -> bool {
        if (channel_registry_ && channel_registry_->should_subscribe_shm(channel)) {
            return true;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (not local_group_.empty()) {
            const auto it = route_registry_.find(channel);
            if (it != route_registry_.end()) {
                return it->second.producer_group != local_group_ && it->second.consumer_groups.find(local_group_) != it->second.consumer_groups.end();
            }
        }
        return shm_sub_channels_.find(channel) != shm_sub_channels_.end();
    }

    auto should_use_shm_for_channel(const std::string& channel) const -> bool { return should_publish_shm(channel) || should_subscribe_shm(channel); }

    static auto parse_channel_set(const char* value) -> std::unordered_set<std::string> { return parse_channel_set(value ? std::string(value) : std::string()); }

    static auto parse_channel_set(const std::string& s) -> std::unordered_set<std::string> {
        std::unordered_set<std::string> out;
        if (s.empty()) {
            return out;
        }
        std::string cur;
        for (const auto ch : s) {
            if (ch == ',' || ch == ';') {
                if (not cur.empty()) {
                    out.emplace(std::move(cur));
                    cur = {};
                }
                continue;
            }
            cur.push_back(ch);
        }
        if (not cur.empty()) {
            out.emplace(std::move(cur));
        }
        return out;
    }

    static auto parse_route_plan(const std::string& s) -> std::unordered_map<std::string, ChannelRegistry::RouteEntry> {
        auto out = std::unordered_map<std::string, ChannelRegistry::RouteEntry>{};
        if (s.empty()) {
            return out;
        }
        size_t start = 0;
        while (start < s.size()) {
            const auto end = s.find(';', start);
            const auto item = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (not item.empty()) {
                const auto p1 = item.find('|');
                const auto p2 = p1 == std::string::npos ? std::string::npos : item.find('|', p1 + 1);
                if (p1 != std::string::npos && p2 != std::string::npos) {
                    ChannelRegistry::RouteEntry entry;
                    entry.producer_group = item.substr(p1 + 1, p2 - p1 - 1);
                    entry.consumer_groups = parse_channel_set(item.substr(p2 + 1));
                    out.emplace(item.substr(0, p1), std::move(entry));
                }
            }
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
        return out;
    }

    std::shared_ptr<ChannelRegistry> channel_registry_;
    std::shared_ptr<NativeMessageBus> inproc_;
    std::shared_ptr<ShmMessageBus> shm_;
    mutable std::mutex mutex_;
    std::string local_group_;
    std::unordered_map<std::string, ChannelRegistry::RouteEntry> route_registry_;
    std::unordered_set<std::string> shm_pub_channels_;
    std::unordered_set<std::string> shm_sub_channels_;
};

}  // namespace star::transport

#endif  // __SHM_ROUTED_MESSAGE_BUS_H__

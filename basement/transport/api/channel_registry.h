#ifndef __CHANNEL_REGISTRY_H__
#define __CHANNEL_REGISTRY_H__

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "basement/transport/common/route_parser.h"

namespace star::transport {

struct ChannelDescriptor;

class ChannelRegistry {
public:
    using RouteEntry = star::transport::RouteEntry;

    enum class ShmRouteMode {
        INPROC_ONLY,
        SHM_WRITE,
        SHM_READ,
        SHM_READ_WRITE,
    };

    void configure_local_group(std::string local_group) {
        std::lock_guard<std::mutex> lock(mutex_);
        local_group_ = std::move(local_group);
    }

    void configure_route_plan(const std::string& route_plan) {
        std::lock_guard<std::mutex> lock(mutex_);
        route_registry_ = parse_route_plan(route_plan);
    }

    void configure_channel_sets(const std::string& shm_pub_channels_csv, const std::string& shm_sub_channels_csv) {
        std::lock_guard<std::mutex> lock(mutex_);
        shm_pub_channels_ = parse_channel_set(shm_pub_channels_csv);
        shm_sub_channels_ = parse_channel_set(shm_sub_channels_csv);
    }

    void register_writer(const std::string& channel, const std::string& process_group) {
        if (channel.empty() || process_group.empty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto& entry = route_registry_[channel];
        if (entry.producer_group.empty()) {
            entry.producer_group = process_group;
        }
    }

    void register_reader(const std::string& channel, const std::string& process_group) {
        if (channel.empty() || process_group.empty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        route_registry_[channel].consumer_groups.insert(process_group);
    }

    auto local_group() const -> std::string {
        std::lock_guard<std::mutex> lock(mutex_);
        return local_group_;
    }

    auto shm_route_mode(const std::string& channel) const -> ShmRouteMode {
        std::lock_guard<std::mutex> lock(mutex_);
        return shm_route_mode_locked(channel);
    }

    auto should_write_shm(const std::string& channel) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto mode = shm_route_mode_locked(channel);
        return mode == ShmRouteMode::SHM_WRITE || mode == ShmRouteMode::SHM_READ_WRITE;
    }

    auto should_read_shm(const std::string& channel) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto mode = shm_route_mode_locked(channel);
        return mode == ShmRouteMode::SHM_READ || mode == ShmRouteMode::SHM_READ_WRITE;
    }

private:
    auto shm_route_mode_locked(const std::string& channel) const -> ShmRouteMode {
        if (not local_group_.empty()) {
            const auto it = route_registry_.find(channel);
            if (it == route_registry_.end()) {
                return ShmRouteMode::INPROC_ONLY;
            }
            if (it->second.producer_group != local_group_) {
                if (it->second.consumer_groups.find(local_group_) != it->second.consumer_groups.end()) {
                    return ShmRouteMode::SHM_READ;
                }
                return ShmRouteMode::INPROC_ONLY;
            }
            for (const auto& consumer_group : it->second.consumer_groups) {
                if (consumer_group != local_group_) {
                    return ShmRouteMode::SHM_WRITE;
                }
            }
            return ShmRouteMode::INPROC_ONLY;
        }

        const bool can_write = shm_pub_channels_.find(channel) != shm_pub_channels_.end();
        const bool can_read = shm_sub_channels_.find(channel) != shm_sub_channels_.end();
        if (can_write && can_read) {
            return ShmRouteMode::SHM_READ_WRITE;
        }
        if (can_write) {
            return ShmRouteMode::SHM_WRITE;
        }
        if (can_read) {
            return ShmRouteMode::SHM_READ;
        }
        return ShmRouteMode::INPROC_ONLY;
    }

    mutable std::mutex mutex_;
    std::string local_group_;
    std::unordered_map<std::string, RouteEntry> route_registry_;
    std::unordered_set<std::string> shm_pub_channels_;
    std::unordered_set<std::string> shm_sub_channels_;
};

}  // namespace star::transport

#endif // __CHANNEL_REGISTRY_H__

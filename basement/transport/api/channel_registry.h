#ifndef __CHANNEL_REGISTRY_H__
#define __CHANNEL_REGISTRY_H__

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace star::transport {

struct ChannelDescriptor;

class ChannelRegistry {
public:
    struct RouteEntry {
        std::string producer_group;
        std::unordered_set<std::string> consumer_groups;
    };

    void configure_local_group(std::string local_group) {
        std::lock_guard<std::mutex> lock(mutex_);
        local_group_ = std::move(local_group);
    }

    void configure_route_plan(const std::string& route_plan) {
        std::lock_guard<std::mutex> lock(mutex_);
        route_registry_ = parse_route_plan(route_plan);
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

    auto should_publish_shm(const std::string& channel) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        if (local_group_.empty()) {
            return false;
        }
        const auto it = route_registry_.find(channel);
        if (it == route_registry_.end() || it->second.producer_group != local_group_) {
            return false;
        }
        for (const auto& consumer_group : it->second.consumer_groups) {
            if (consumer_group != local_group_) {
                return true;
            }
        }
        return false;
    }

    auto should_subscribe_shm(const std::string& channel) const -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
        if (local_group_.empty()) {
            return false;
        }
        const auto it = route_registry_.find(channel);
        if (it == route_registry_.end()) {
            return false;
        }
        return it->second.producer_group != local_group_
            && it->second.consumer_groups.find(local_group_) != it->second.consumer_groups.end();
    }

private:
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

    static auto parse_route_plan(const std::string& s) -> std::unordered_map<std::string, RouteEntry> {
        auto out = std::unordered_map<std::string, RouteEntry>{};
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
                    RouteEntry entry;
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

    mutable std::mutex mutex_;
    std::string local_group_;
    std::unordered_map<std::string, RouteEntry> route_registry_;
};

}  // namespace star::transport

#endif // __CHANNEL_REGISTRY_H__

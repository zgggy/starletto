#ifndef __TRANSPORT_ROUTE_PARSER_H__
#define __TRANSPORT_ROUTE_PARSER_H__

#include "basement/core/exception.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace star::transport {

struct RouteEntry {
    std::string producer_group;
    std::unordered_set<std::string> consumer_groups;
};

inline auto parse_channel_set(const std::string& s) -> std::unordered_set<std::string> {
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

inline auto parse_channel_set(const char* value) -> std::unordered_set<std::string> {
    return parse_channel_set(value ? std::string(value) : std::string());
}

inline auto parse_route_plan(const std::string& s) -> std::unordered_map<std::string, RouteEntry> {
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
            if (p1 == std::string::npos || p2 == std::string::npos) {
                throw star::excep::RuntimeError("parse_route_plan failed, invalid entry (expected channel|producer|consumers): ", item);
            }
            const auto channel = item.substr(0, p1);
            const auto producer_group = item.substr(p1 + 1, p2 - p1 - 1);
            if (channel.empty() || producer_group.empty()) {
                throw star::excep::RuntimeError("parse_route_plan failed, empty channel/producer_group in entry: ", item);
            }
            RouteEntry entry;
            entry.producer_group = producer_group;
            entry.consumer_groups = parse_channel_set(item.substr(p2 + 1));
            out.emplace(channel, std::move(entry));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return out;
}

}  // namespace star::transport

#endif  // __TRANSPORT_ROUTE_PARSER_H__

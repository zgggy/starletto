#ifndef __TRANSPORT_QOS_MANAGER_H__
#define __TRANSPORT_QOS_MANAGER_H__

#include <deque>

#include "basement/transport/api/message_bus.h"
#include "basement/transport/common/channel_stats.h"

namespace star::transport {

struct QosApplyResult {
    bool should_deliver{true};
};

inline auto apply_qos(std::deque<MessageView>& history, const MessageView& message, const ChannelOptions& options, ChannelStats& stats) -> QosApplyResult {
    if (options.depth == 0) {
        return QosApplyResult{true};
    }
    if (options.depth == 1) {
        if (options.drop_policy == DropPolicy::KEEP_LAST) {
            if (history.empty()) {
                history.push_back(message);
            } else {
                stats.record_drop(1);
                history.back() = message;
            }
            stats.update_queue_depth(history.size());
            return QosApplyResult{true};
        }
        if (!history.empty()) {
            stats.record_drop(1);
            return QosApplyResult{false};
        }
        history.push_back(message);
        stats.update_queue_depth(history.size());
        return QosApplyResult{true};
    }
    if (options.drop_policy == DropPolicy::KEEP_LAST) {
        history.push_back(message);
        while (history.size() > options.depth) {
            history.pop_front();
            stats.record_drop(1);
        }
    } else {
        if (history.size() >= options.depth) {
            stats.record_drop(1);
            return QosApplyResult{false};
        }
        history.push_back(message);
    }
    stats.update_queue_depth(history.size());
    return QosApplyResult{true};
}

}  // namespace star::transport

#endif  // __TRANSPORT_QOS_MANAGER_H__

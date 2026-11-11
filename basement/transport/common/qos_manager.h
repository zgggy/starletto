#ifndef __TRANSPORT_QOS_MANAGER_H__
#define __TRANSPORT_QOS_MANAGER_H__

#include <deque>

#include "basement/core/exception.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/common/channel_stats.h"

namespace star::transport {

// DROP_NEWEST 需要「消费者已消费」的信号才能实现。当前通道没有背压/消费通知，
// 历史队列永远不会被消费，填满后所有新消息会被永久丢弃（静默全丢数据）。
// 与其静默停投，不如在声明通道时明确拒绝。
inline void validate_qos_options(const ChannelOptions& options, const std::string& channel) {
    if (options.drop_policy == DropPolicy::DROP_NEWEST) {
        throw star::excep::RuntimeError("channel option unsupported: drop_policy=DROP_NEWEST is not implemented (no consumer/backpressure signal), use KEEP_LAST. channel=",
                                        channel);
    }
}

struct QosApplyResult {
    bool should_deliver{true};
};

inline auto apply_qos(std::deque<MessageView>& history, const MessageView& message, const ChannelOptions& options, ChannelStats& stats) -> QosApplyResult {
    if (options.depth == 0) {
        return QosApplyResult{true};
    }
    // 历史队列只服务于 peek_latest（保留最近 N 条），没有「消费」动作。
    // DROP_NEWEST 需要消费/背压信号才能正确实现，declare_channel 已明确拒绝该配置；
    // 万一被绕过，这里退化为 KEEP_LAST —— 宁可多收，绝不静默停投。
    if (options.depth == 1) {
        if (history.empty()) {
            history.push_back(message);
        } else {
            stats.record_drop(1);
            history.back() = message;
        }
        stats.update_queue_depth(history.size());
        return QosApplyResult{true};
    }
    history.push_back(message);
    while (history.size() > options.depth) {
        history.pop_front();
        stats.record_drop(1);
    }
    stats.update_queue_depth(history.size());
    return QosApplyResult{true};
}

}  // namespace star::transport

#endif  // __TRANSPORT_QOS_MANAGER_H__

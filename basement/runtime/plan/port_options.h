#ifndef __STAR_RUNTIME_PLAN_PORT_OPTIONS_H__
#define __STAR_RUNTIME_PLAN_PORT_OPTIONS_H__

#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/decode_policy.h"
#include "basement/transport/api/message_bus.h"

namespace star::runtime::plan {

// component.conf 的 decode_policy 块 → 运行时策略。
// 取值非法属于装配错误，直接抛异常并带通道名。
inline auto decode_policy_for(const star::generated::runtime::component_load_plan::PortPlan& endpoint) -> star::schema::DecodePolicy {
    auto policy = star::schema::DecodePolicy{};
    if (not endpoint.decode_policy.has_value()) {
        return policy;
    }
    const auto& conf = *endpoint.decode_policy;
    const auto unknown = conf.unknown.value_or("");
    const auto missing = conf.missing.value_or("");
    const auto mismatch = conf.type_mismatch.value_or("");
    if (not star::schema::parse_decode_policy(unknown, missing, mismatch, &policy)) {
        throw star::excep::RuntimeError("component decode_policy invalid: channel=",
                                        endpoint.channel.value_or(endpoint.name),
                                        ", unknown=", unknown,
                                        ", missing=", missing,
                                        ", type_mismatch=", mismatch);
    }
    return policy;
}

// 把端口上的 QoS/解码策略写进通道描述符。普通 reader 与 trigger reader 共用，
// 避免 trigger 管线漏掉 decode_policy/QoS。
inline void apply_port_options(const star::generated::runtime::component_load_plan::PortPlan& endpoint, star::transport::ChannelDescriptor& descriptor) {
    if (endpoint.has_qos_depth()) {
        descriptor.options.depth = static_cast<uint32_t>(endpoint.qos_depth.value());
    }
    if (endpoint.has_qos_drop_policy() && endpoint.qos_drop_policy.value() == "drop_newest") {
        descriptor.options.drop_policy = star::transport::DropPolicy::DROP_NEWEST;
    }
    descriptor.options.decode_policy = decode_policy_for(endpoint);
}

}  // namespace star::runtime::plan

#endif  // __STAR_RUNTIME_PLAN_PORT_OPTIONS_H__

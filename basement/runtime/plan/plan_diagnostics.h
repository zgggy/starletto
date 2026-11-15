#ifndef __STAR_RUNTIME_PLAN_DIAGNOSTICS_H__
#define __STAR_RUNTIME_PLAN_DIAGNOSTICS_H__

#include <map>
#include <string>
#include <vector>

#include "basement/runtime/plan/plan_builder.h"

namespace star::runtime::plan {

// 静态拓扑检查：同一通道在生产者/消费者两侧声明的类型必须一致。
// 这些错误现在只能在首次写入/解码时暴露，启动期就能全部查出来。
inline auto find_channel_type_conflicts(const std::vector<ComponentLoadPlan>& plans) -> std::vector<std::string> {
    auto declarations = std::map<std::string, std::map<std::string, std::string>>{};

    const auto declare = [&declarations](const std::string& channel, const std::string& type_name, const std::string& source) {
        if (channel.empty() or type_name.empty()) {
            return;
        }
        declarations[channel].emplace(type_name, source);
    };

    for (const auto& plan : plans) {
        const auto owner = plan.module_name + "." + plan.component_name;
        if (plan.outputs.has_value()) {
            for (const auto& port : *plan.outputs) {
                const auto channel = port.channel.value_or("");
                const auto type_name = port.type_name.value_or("");
                declare(channel, type_name, owner + ".writers." + port.name);
            }
        }
        if (plan.inputs.has_value()) {
            for (const auto& port : *plan.inputs) {
                const auto channel = port.channel.value_or("");
                const auto type_name = port.type_name.value_or("");
                declare(channel, type_name, owner + ".readers." + port.name);
            }
        }
        if (plan.services.has_value()) {
            for (const auto& service : *plan.services) {
                const auto channel = service.channel.value_or(service.name);
                declare(channel + "/request", service.request_type_name.value_or(""), owner + ".commands." + service.name);
                declare(channel + "/response", service.response_type_name.value_or(""), owner + ".commands." + service.name);
            }
        }
    }

    auto issues = std::vector<std::string>{};
    for (const auto& [channel, types] : declarations) {
        if (types.size() <= 1) {
            continue;
        }
        auto message = "channel '" + channel + "' has conflicting schema types:";
        for (const auto& [type, source] : types) {
            message += " '" + type + "' (" + source + ")";
        }
        issues.push_back(std::move(message));
    }
    return issues;
}

}  // namespace star::runtime::plan

#endif  // __STAR_RUNTIME_PLAN_DIAGNOSTICS_H__

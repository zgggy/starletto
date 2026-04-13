#ifndef __STAR_RUNTIME_PLAN_PLAN_BUILDER_H__
#define __STAR_RUNTIME_PLAN_PLAN_BUILDER_H__

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <unordered_set>
#include <vector>

#include "basement/schema/builtin/manifest/component.s.h"
#include "basement/schema/builtin/manifest/module.s.h"
#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/builtin/runtime/process_plan.s.h"

namespace star::runtime::plan {

using ComponentLoadPlan = star::generated::runtime::component_load_plan::ComponentLoadPlan;
using PortPlan = star::generated::runtime::component_load_plan::PortPlan;
using TriggerPlan = star::generated::runtime::component_load_plan::TriggerPlan;
using ProcessPlan = star::generated::runtime::process_plan::ProcessPlan;
using ProcessGroupPlan = star::generated::runtime::process_plan::ProcessGroupPlan;

using TaskSchedulePlan = star::generated::runtime::process_plan::TaskSchedulePlan;
using ChoreographyPlan = star::generated::runtime::process_plan::ChoreographyPlan;

using ComponentConf = star::generated::manifest::component::ComponentConf;
using ModuleConf = star::generated::manifest::module::ModuleConf;

struct LoadContext {
    std::filesystem::path component_conf_path;
    std::filesystem::path module_conf_path;
    std::filesystem::path module_dir;
    std::filesystem::path module_config_dir;
    std::string module_name;
};

inline auto build_component_load_plan(const ComponentConf& conf, const LoadContext& ctx) -> ComponentLoadPlan {
    auto plan = ComponentLoadPlan{};
    plan.module_name = ctx.module_name.empty() ? conf.component.type : ctx.module_name;
    plan.component_name = conf.component.name;
    if (conf.component.component_group.has_value()) {
        plan.component_group = *conf.component.component_group;
    }
    plan.component_class = conf.component.component_class;
    if (conf.component.component_library.has_value()) {
        plan.component_library = *conf.component.component_library;
    }
    plan.static_link = conf.component.static_link;
    if (conf.component.priority.has_value()) {
        plan.priority = *conf.component.priority;
    }
    if (not ctx.component_conf_path.empty()) {
        plan.component_conf_path = std::filesystem::absolute(ctx.component_conf_path).string();
    }
    if (not ctx.module_conf_path.empty()) {
        plan.module_conf_path = std::filesystem::absolute(ctx.module_conf_path).string();
    }
    if (not ctx.module_dir.empty()) {
        plan.module_dir = std::filesystem::absolute(ctx.module_dir).string();
    }
    if (not ctx.module_config_dir.empty()) {
        plan.module_config_dir = std::filesystem::absolute(ctx.module_config_dir).string();
    }
    if (conf.component.config_file_path.has_value()) {
        plan.config_file_path = *conf.component.config_file_path;
    }

    if (conf.global.has_value()) {
        if (conf.global->process_group.has_value()) {
            plan.process_group = *conf.global->process_group;
        }
        if (conf.global->scheduler_policy.has_value()) {
            plan.scheduler_type = *conf.global->scheduler_policy;
        }
        if (conf.global->executor_threads.has_value()) {
            plan.executor_threads = *conf.global->executor_threads;
        }
    }

    if (conf.trigger.has_value()) {
        auto trigger = TriggerPlan{};
        if (conf.trigger->type.has_value()) {
            trigger.type = *conf.trigger->type;
        }
        if (conf.trigger->task_name.has_value()) {
            trigger.task_name = *conf.trigger->task_name;
        }
        if (conf.trigger->cycle_ms.has_value()) {
            trigger.cycle_ms = *conf.trigger->cycle_ms;
        }
        if (conf.trigger->readers.has_value()) {
            trigger.readers = *conf.trigger->readers;
        }
        plan.trigger = std::move(trigger);
    }

    if (conf.readers.has_value()) {
        auto inputs = std::vector<PortPlan>{};
        inputs.reserve(conf.readers->size());
        for (const auto& [name, port] : *conf.readers) {
            auto item = PortPlan{};
            item.name = name;
            item.channel = port.channel;
            if (port.type.has_value()) {
                item.type_name = *port.type;
            } else if (port.msg_type.has_value()) {
                item.type_name = *port.msg_type;
            }
            if (port.pending_queue_size.has_value()) {
                item.qos_depth = *port.pending_queue_size;
            }
            if (port.qos_profile.has_value() && port.qos_profile->depth.has_value()) {
                item.qos_depth = *port.qos_profile->depth;
            }
            if (port.qos_profile.has_value() && port.qos_profile->history.has_value()) {
                item.qos_drop_policy = *port.qos_profile->history;
            }
            inputs.emplace_back(std::move(item));
        }
        plan.inputs = std::move(inputs);
    }

    if (conf.writers.has_value()) {
        auto outputs = std::vector<PortPlan>{};
        outputs.reserve(conf.writers->size());
        for (const auto& [name, port] : *conf.writers) {
            auto item = PortPlan{};
            item.name = name;
            item.channel = port.channel;
            if (port.msg_type.has_value()) {
                item.type_name = *port.msg_type;
            } else if (port.type.has_value()) {
                item.type_name = *port.type;
            }
            if (port.record.has_value()) {
                item.record = *port.record;
            }
            if (port.pending_queue_size.has_value()) {
                item.qos_depth = *port.pending_queue_size;
            }
            if (port.qos_profile.has_value() && port.qos_profile->depth.has_value()) {
                item.qos_depth = *port.qos_profile->depth;
            }
            if (port.qos_profile.has_value() && port.qos_profile->history.has_value()) {
                item.qos_drop_policy = *port.qos_profile->history;
            }
            outputs.emplace_back(std::move(item));
        }
        plan.outputs = std::move(outputs);
    }

    if (conf.commands.has_value()) {
        auto services = std::vector<star::generated::runtime::component_load_plan::ServicePortPlan>{};
        services.reserve(conf.commands->size());
        for (const auto& [name, port] : *conf.commands) {
            auto item = star::generated::runtime::component_load_plan::ServicePortPlan{};
            item.name = name;
            item.channel = port.channel;
            if (port.type.has_value()) {
                item.request_type_name = *port.type;
            } else if (port.msg_type.has_value()) {
                item.request_type_name = *port.msg_type;
            }
            if (port.ack_type.has_value()) {
                item.response_type_name = *port.ack_type;
            }
            services.emplace_back(std::move(item));
        }
        plan.services = std::move(services);
    }

    return plan;
}

inline auto build_process_plan(const std::vector<ComponentLoadPlan>& component_plans) -> ProcessPlan {
    auto grouped = std::map<std::string, ProcessGroupPlan>{};

    for (const auto& plan : component_plans) {
        const auto group_name = plan.process_group.value_or("default");
        auto& group = grouped[group_name];
        if (group.name.empty()) {
            group.name = group_name;
        }

        if (not group.scheduler_type.has_value() and plan.scheduler_type.has_value()) {
            group.scheduler_type = plan.scheduler_type;
        }

        if (not group.component_names.has_value()) {
            group.component_names = std::vector<std::string>{};
        }
        group.component_names->emplace_back(plan.component_name);

        if (plan.scheduler_type.value_or("") == "choreography" or plan.priority.value_or(0) > 0) {
            if (not group.choreography.has_value()) {
                group.choreography = ChoreographyPlan{};
                group.choreography->pool_threads = plan.executor_threads.value_or(1);
                group.choreography->tasks = std::vector<TaskSchedulePlan>{};
            }
            auto task = TaskSchedulePlan{};
            task.name = plan.component_name;
            task.priority = plan.priority.value_or(0);
            group.choreography->tasks->emplace_back(std::move(task));
        }

        if (plan.component_conf_path.has_value()) {
            if (not group.component_conf_paths.has_value()) {
                group.component_conf_paths = std::vector<std::string>{};
            }
            group.component_conf_paths->emplace_back(*plan.component_conf_path);
        }
        if (plan.outputs.has_value()) {
            if (not group.produced_channels.has_value()) {
                group.produced_channels = std::vector<std::string>{};
            }
            for (const auto& port : *plan.outputs) {
                if (port.channel.has_value()) {
                    group.produced_channels->emplace_back(*port.channel);
                }
            }
        }
        if (plan.inputs.has_value()) {
            if (not group.consumed_channels.has_value()) {
                group.consumed_channels = std::vector<std::string>{};
            }
            for (const auto& port : *plan.inputs) {
                if (port.channel.has_value()) {
                    group.consumed_channels->emplace_back(*port.channel);
                }
            }
        }
    }

    auto dedupe = [](std::vector<std::string>* values) {
        if (values == nullptr) {
            return;
        }
        std::sort(values->begin(), values->end());
        values->erase(std::unique(values->begin(), values->end()), values->end());
    };

    auto process_plan = ProcessPlan{};
    process_plan.groups = std::vector<ProcessGroupPlan>{};
    process_plan.groups->reserve(grouped.size());
    for (auto& [_, group] : grouped) {
        if (group.component_names.has_value()) {
            dedupe(&*group.component_names);
        }
        if (group.component_conf_paths.has_value()) {
            dedupe(&*group.component_conf_paths);
        }
        if (group.produced_channels.has_value()) {
            dedupe(&*group.produced_channels);
        }
        if (group.consumed_channels.has_value()) {
            dedupe(&*group.consumed_channels);
        }
        process_plan.groups->emplace_back(std::move(group));
    }
    return process_plan;
}

}  // namespace star::runtime::plan

#endif  // __STAR_RUNTIME_PLAN_PLAN_BUILDER_H__

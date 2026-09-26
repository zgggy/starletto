#ifndef __STAR_RUNTIME_COMPONENT_NATIVE_COMPONENT_HOST_H__
#define __STAR_RUNTIME_COMPONENT_NATIVE_COMPONENT_HOST_H__

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_dl_helper.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/runtime/plan/plan_builder.h"
#include "basement/runtime/scheduler/native_scheduler.h"
#include "basement/runtime/scheduler/scheduler.h"
#include "basement/runtime/trigger/trigger.h"
#include "basement/transport/api/channel_registry.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/record/message_recorder.h"

namespace star::runtime {

class NativeComponentHost final {
  public:
    NativeComponentHost(RuntimeContextPtr context, std::shared_ptr<ComponentRegistry> registry) : context_(std::move(context)), registry_(std::move(registry)) {
        if (not context_) {
            throw star::excep::RuntimeError("NativeComponentHost init failed, context is null");
        }
        if (not registry_) {
            throw star::excep::RuntimeError("NativeComponentHost init failed, registry is null");
        }
    }

    void load(const star::runtime::plan::ComponentLoadPlan& plan) {
        maybe_load_dynamic_library(plan);
        auto component = registry_->create(plan.component_class);
        if (not component) {
            throw star::excep::RuntimeError("NativeComponentHost load failed, create component failed: ", plan.component_class);
        }
        register_plan_ports(plan);
        component->set_context(context_);
        component->set_load_plan(plan);
        components_.push_back(std::move(component));
        plans_.push_back(plan);
    }

    void set_process_plan(const star::runtime::plan::ProcessPlan& plan) { process_plan_ = plan; }

    void configure() {
        if (tasks_initialized_) {
            return;
        }
        tasks_initialized_ = true;
        apply_process_plan();
        setup_runtime_tasks();
    }

    void start() {
        setup_recorder();
        for (auto& c : components_) {
            if (not c) {
                continue;
            }
            try {
                c->auto_configure();
                c->init();
            } catch (const std::exception& e) {
                logE << "NativeComponentHost start failed: " << e.what();
            } catch (...) {
                logE << "NativeComponentHost start failed: unknown exception";
            }
        }
        for (auto& [group, scheduler] : schedulers_) {
            if (not scheduler) {
                continue;
            }
            try {
                scheduler->start();
            } catch (const std::exception& e) {
                logE << "NativeComponentHost scheduler start failed, group=" << group << ", err: " << e.what();
            } catch (...) {
                logE << "NativeComponentHost scheduler start failed, group=" << group << ", err: unknown exception";
            }
        }
    }

    void stop() {
        for (auto& [group, scheduler] : schedulers_) {
            if (not scheduler) {
                continue;
            }
            try {
                scheduler->stop();
            } catch (const std::exception& e) {
                logE << "NativeComponentHost scheduler stop failed, group=" << group << ", err: " << e.what();
            } catch (...) {
                logE << "NativeComponentHost scheduler stop failed, group=" << group << ", err: unknown exception";
            }
        }
        for (auto& c : components_) {
            if (not c) {
                continue;
            }
            try {
                c->shutdown();
            } catch (const std::exception& e) {
                logE << "NativeComponentHost stop failed: " << e.what();
            } catch (...) {
                logE << "NativeComponentHost stop failed: unknown exception";
            }
        }
    }

    auto wait_for_frames(int64_t frames) -> bool { return wait_for_frames(frames, 0); }

    auto wait_for_frames(int64_t frames, int64_t timeout_ms) -> bool {
        if (frames <= 0) {
            return true;
        }
        std::unique_lock<std::mutex> lock(wait_mutex_);
        if (timeout_ms <= 0) {
            wait_cv_.wait(lock, [this, frames]() { return executed_frames_.load(std::memory_order_relaxed) >= frames; });
            return true;
        }
        const auto ok = wait_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this, frames]() { return executed_frames_.load(std::memory_order_relaxed) >= frames; });
        return ok;
    }

    auto process_once() -> bool {
        bool ok = true;
        for (auto& c : components_) {
            if (not c) {
                ok = false;
                continue;
            }
            try {
                ok = c->process_once() && ok;
            } catch (const std::exception& e) {
                logE << "NativeComponentHost process_once failed: " << e.what();
                ok = false;
            } catch (...) {
                logE << "NativeComponentHost process_once failed: unknown exception";
                ok = false;
            }
        }
        return ok;
    }

  private:
    using RegisterComponentsFn = void (*)(star::runtime::ComponentRegistry&);

    void register_plan_ports(const star::runtime::plan::ComponentLoadPlan& plan) {
        const auto group = plan.process_group.value_or("default");
        context_->channel_registry().configure_local_group(group);
        if (plan.outputs.has_value()) {
            for (const auto& port : *plan.outputs) {
                if (port.channel.has_value()) {
                    context_->channel_registry().register_writer(*port.channel, group);
                }
            }
        }
        if (plan.inputs.has_value()) {
            for (const auto& port : *plan.inputs) {
                if (port.channel.has_value()) {
                    context_->channel_registry().register_reader(*port.channel, group);
                }
            }
        }
    }

    void apply_process_plan() {
        if (not process_plan_.has_value() || not process_plan_->groups.has_value()) {
            return;
        }
        build_schedulers_by_process_group();
        for (const auto& group_plan : *process_plan_->groups) {
            auto scheduler = find_scheduler(group_plan.name);
            if (scheduler) {
                scheduler->apply_plan(group_plan);
            }
        }
    }

    void setup_runtime_tasks() {
        if (schedulers_.empty()) {
            build_schedulers_by_process_group();
        }

        for (size_t i = 0; i < components_.size() && i < plans_.size(); ++i) {
            const auto& p = plans_[i];
            auto task_name = resolve_task_name_from_plan(p);
            const auto group = p.process_group.value_or("default");
            auto scheduler = find_scheduler(group);
            if (not scheduler) {
                continue;
            }

            const auto is_timer_trigger = p.trigger.has_value() && (p.trigger->type.value_or("") == "timer" || (p.trigger->type.value_or("").empty() && p.trigger->cycle_ms.value_or(0) > 0));
            const auto is_channel_trigger =
                p.trigger.has_value() && (p.trigger->type.value_or("") == "channel" || (p.trigger->type.value_or("").empty() && p.trigger->readers.has_value() && !p.trigger->readers->empty()));
            const auto is_sync_channel_trigger = p.trigger.has_value() && (p.trigger->type.value_or("") == "sync_topic" ||
                                                                           (p.trigger->type.value_or("") == "channel" && p.trigger->readers.has_value() && p.trigger->readers->size() > 1));

            star::runtime::TaskDescriptor desc;
            desc.name = task_name;
            desc.policy = p.scheduler_type.value_or("");
            desc.period_ms = is_timer_trigger ? p.trigger->cycle_ms.value_or(0) : 0;
            desc.priority = p.priority.value_or(0);
            desc.callback = [this, idx = i](int64_t frame_id) {
                bool ok = true;
                auto& c = components_[idx];
                if (not c) {
                    ok = false;
                } else {
                    try {
                        ok = c->process_once();
                    } catch (const std::exception& e) {
                        logE << "NativeComponentHost task process_once failed: " << e.what();
                        ok = false;
                    } catch (...) {
                        logE << "NativeComponentHost task process_once failed: unknown exception";
                        ok = false;
                    }
                }
                executed_frames_.fetch_add(1, std::memory_order_relaxed);
                {
                    std::lock_guard<std::mutex> lock(wait_mutex_);
                    last_frame_id_ = frame_id;
                }
                wait_cv_.notify_all();
                return ok;
            };
            scheduler->register_task(desc);
            task_schedulers_[task_name] = scheduler;

            if (is_channel_trigger && not is_sync_channel_trigger) {
                setup_channel_trigger_readers_from_plan(task_name, p, false);
            }
            if (is_sync_channel_trigger) {
                setup_channel_trigger_readers_from_plan(task_name, p, true);
            }
        }
    }

    void setup_channel_trigger_readers_from_plan(const std::string& task_name, const star::runtime::plan::ComponentLoadPlan& plan, bool sync_mode) {
        if (not plan.trigger.has_value() || not plan.trigger->readers.has_value()) {
            return;
        }
        if (sync_mode) {
            auto sync_trigger = std::make_shared<star::runtime::SyncTrigger>([this, task_name](int64_t frame_id) {
                auto scheduler = find_task_scheduler(task_name);
                if (scheduler) {
                    scheduler->notify(task_name, frame_id);
                }
            });
            for (const auto& reader_name : *plan.trigger->readers) {
                const auto port = find_input_port_from_plan(plan, reader_name);
                if (not port.has_value()) {
                    continue;
                }
                sync_trigger->add_required_channel(port->channel.value_or(""));
                star::transport::ChannelDescriptor d;
                d.name = port->channel.value_or("");
                d.type_name = port->type_name.value_or("");
                const auto channel = d.name;
                auto reader = context_->message_bus().create_reader(d, [sync_trigger, channel](const star::transport::MessageView&) { sync_trigger->on_message(channel); });
                if (reader) {
                    trigger_readers_.push_back(std::move(reader));
                }
            }
            sync_triggers_[task_name] = std::move(sync_trigger);
            logI << "SyncTrigger created for task=" << task_name << ", required_channels=" << sync_triggers_[task_name]->required_count();
            return;
        }
        for (const auto& reader_name : *plan.trigger->readers) {
            const auto port = find_input_port_from_plan(plan, reader_name);
            if (not port.has_value()) {
                continue;
            }
            star::transport::ChannelDescriptor d;
            d.name = port->channel.value_or("");
            d.type_name = port->type_name.value_or("");
            auto reader = context_->message_bus().create_reader(d, [this, task_name](const star::transport::MessageView&) {
                const auto frame_id = next_trigger_frame_id(task_name);
                auto scheduler = find_task_scheduler(task_name);
                if (scheduler) {
                    scheduler->notify(task_name, frame_id);
                }
            });
            if (reader) {
                trigger_readers_.push_back(std::move(reader));
            }
        }
    }

    static auto find_input_port_from_plan(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& name) -> std::optional<star::runtime::plan::PortPlan> {
        if (not plan.inputs.has_value()) {
            return std::nullopt;
        }
        for (const auto& e : *plan.inputs) {
            if (e.name == name) {
                return e;
            }
        }
        return std::nullopt;
    }

    auto next_trigger_frame_id(const std::string& task_name) -> int64_t {
        std::lock_guard<std::mutex> lock(trigger_frame_mutex_);
        auto& v = trigger_frame_ids_[task_name];
        v += 1;
        return v;
    }

    static auto resolve_task_name_from_plan(const star::runtime::plan::ComponentLoadPlan& plan) -> std::string {
        if (plan.trigger.has_value() && plan.trigger->task_name.has_value() && not plan.trigger->task_name->empty()) {
            return *plan.trigger->task_name;
        }
        if (plan.module_name.empty()) {
            return plan.component_name;
        }
        if (plan.component_name.empty()) {
            return plan.module_name;
        }
        return plan.module_name + "." + plan.component_name;
    }

    void setup_recorder() {
        std::vector<std::string> record_channels;
        for (const auto& p : plans_) {
            if (not p.outputs.has_value()) {
                continue;
            }
            for (const auto& port : *p.outputs) {
                if (port.record.value_or(false) && port.channel.has_value() && not port.channel->empty()) {
                    record_channels.push_back(*port.channel);
                }
            }
        }
        if (record_channels.empty()) {
            return;
        }
        for (const auto& channel : record_channels) {
            if (auto* recordable = context_->recordable_bus()) {
                recordable->enable_record_channel(channel);
            }
        }
        if (auto* recordable = context_->recordable_bus()) {
            recordable->enable_recorder("/tmp/starletto_record.bin");
        }
        logI << "Recorder enabled, file=/tmp/starletto_record.bin, channels=" << record_channels.size();
    }

    void build_schedulers_by_process_group() {
        std::unordered_map<std::string, int64_t> group_threads;
        for (const auto& p : plans_) {
            const auto group = p.process_group.value_or("default");
            auto& v = group_threads[group];
            const auto threads = p.executor_threads.value_or(0);
            if (threads > v) {
                v = threads;
            }
        }
        if (process_plan_.has_value() && process_plan_->groups.has_value()) {
            for (const auto& gp : *process_plan_->groups) {
                auto& v = group_threads[gp.name];
                if (gp.choreography.has_value() && gp.choreography->pool_threads.has_value()) {
                    if (*gp.choreography->pool_threads > v) {
                        v = *gp.choreography->pool_threads;
                    }
                }
            }
        }
        for (const auto& [group, threads] : group_threads) {
            auto scheduler = std::make_shared<star::runtime::NativeScheduler>();
            star::runtime::SchedulerConfig config;
            config.worker_threads = threads > 0 ? threads : 1;
            scheduler->configure(config);
            scheduler->set_context(context_.get());
            schedulers_[group] = std::move(scheduler);
        }
        if (schedulers_.find("default") == schedulers_.end()) {
            auto scheduler = std::make_shared<star::runtime::NativeScheduler>();
            scheduler->set_context(context_.get());
            schedulers_["default"] = std::move(scheduler);
        }
    }

    auto find_scheduler(const std::string& group) -> std::shared_ptr<star::runtime::Scheduler> {
        const auto it = schedulers_.find(group);
        if (it != schedulers_.end()) {
            return it->second;
        }
        const auto fallback = schedulers_.find("default");
        if (fallback != schedulers_.end()) {
            return fallback->second;
        }
        return {};
    }

    auto find_task_scheduler(const std::string& task_name) -> std::shared_ptr<star::runtime::Scheduler> {
        const auto it = task_schedulers_.find(task_name);
        if (it != task_schedulers_.end()) {
            return it->second;
        }
        return find_scheduler("default");
    }

    void maybe_load_dynamic_library(const star::runtime::plan::ComponentLoadPlan& plan) {
        if (registry_->contains(plan.component_class)) {
            return;
        }
        if (plan.static_link.value_or(false)) {
            return;
        }
        if (plan.component_library.value_or("").empty()) {
            return;
        }
        dl_helper_.load_dynamic_library(plan, dynamic_library_handles_, registry_.get());
    }

    RuntimeContextPtr context_;
    ComponentDlHelper dl_helper_;
    std::unordered_map<std::string, DlHandle> dynamic_library_handles_;
    std::shared_ptr<ComponentRegistry> registry_;
    std::vector<std::unique_ptr<Component>> components_;
    std::vector<star::runtime::plan::ComponentLoadPlan> plans_;
    std::optional<star::runtime::plan::ProcessPlan> process_plan_;
    std::mutex trigger_frame_mutex_;
    std::unordered_map<std::string, int64_t> trigger_frame_ids_;
    std::vector<std::shared_ptr<star::transport::Reader>> trigger_readers_;
    std::unordered_map<std::string, std::shared_ptr<star::runtime::SyncTrigger>> sync_triggers_;
    std::atomic<int64_t> executed_frames_{0};
    std::mutex wait_mutex_;
    std::condition_variable wait_cv_;
    int64_t last_frame_id_{0};
    bool tasks_initialized_{false};
    std::unordered_map<std::string, std::shared_ptr<star::runtime::Scheduler>> schedulers_;
    std::unordered_map<std::string, std::shared_ptr<star::runtime::Scheduler>> task_schedulers_;
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_NATIVE_COMPONENT_HOST_H__

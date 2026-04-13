#ifndef __STAR_RUNTIME_SCHEDULER_SCHEDULER_H__
#define __STAR_RUNTIME_SCHEDULER_SCHEDULER_H__

#include "basement/schema/builtin/runtime/process_plan.s.h"

#include <cstdint>
#include <functional>
#include <string>

namespace star::runtime {

class RuntimeContext;

using ProcessGroupPlan = star::generated::runtime::process_plan::ProcessGroupPlan;

struct TaskDescriptor {
    std::string name;
    std::string policy;
    int64_t period_ms{0};
    int64_t priority{0};
    std::function<bool(int64_t frame_id)> callback;
};

struct SchedulerConfig {
    int64_t worker_threads{0};
};

class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void configure(const SchedulerConfig& config) = 0;
    virtual void apply_plan(const ProcessGroupPlan& plan) = 0;

    virtual void register_task(const TaskDescriptor& descriptor) = 0;
    virtual void notify(const std::string& task_name, int64_t frame_id) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void set_context(RuntimeContext* ctx) = 0;
};

}  // namespace star::runtime

#endif // __STAR_RUNTIME_SCHEDULER_SCHEDULER_H__

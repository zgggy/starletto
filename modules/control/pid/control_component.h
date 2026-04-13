#ifndef __CONTROL_COMPONENT_H__
#define __CONTROL_COMPONENT_H__

#include <algorithm>
#include <chrono>
#include <memory>
#include <mutex>

#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "modules/control/pid/pid_controller.h"
#include "modules/setting/schema/control/command.s.h"
#include "modules/setting/schema/planning/trajectory.s.h"

namespace star::control {

class ControlComponent : public star::runtime::Component {
  public:
    void init() override {
        declare_reader<star::setting::planning::TrajectoryOutput>("planning_trajectory", [this](auto traj) { on_trajectory(traj); });
        logI << "ControlComponent initialized, subscribed to planning.trajectory, publishing control.command";
    }

    auto process_once() -> bool override {
        const auto& frame = context()->current_frame();
        if (frame.frame_id > 0) {
            logI << "ControlComponent processing frame=" << frame.frame_id;
        }
        if (not latest_trajectory_) {
            return true;
        }
        auto now_ns = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        auto e2e_latency_us = (now_ns - latest_trajectory_->timestamp_ns) / 1000.0;
        record_latency(e2e_latency_us);
        auto cmd = controller_.compute(*latest_trajectory_, current_velocity_);
        logI << "ControlCommand: throttle=" << cmd.throttle << ", brake=" << cmd.brake << ", steering=" << cmd.steering << ", gear=" << cmd.gear
             << ", target_vel=" << (latest_trajectory_->points.empty() ? 0.0 : latest_trajectory_->points.front().velocity) << ", e2e_latency_us=" << static_cast<int64_t>(e2e_latency_us);
        write("command", cmd);
        current_velocity_ = cmd.throttle > 0 ? cmd.throttle * 10.0 : std::max(0.0, current_velocity_ - cmd.brake * 5.0);
        maybe_dump_latency_stats();
        return true;
    }

  private:
    void on_trajectory(std::shared_ptr<const star::setting::planning::TrajectoryOutput> traj) { latest_trajectory_ = std::move(traj); }

    void record_latency(double latency_us) {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        ++latency_count_;
        latency_sum_ += latency_us;
        latency_max_ = std::max(latency_max_, latency_us);
        latency_min_ = std::min(latency_min_, latency_us);
    }

    void maybe_dump_latency_stats() {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        if (latency_count_ % kStatsDumpInterval != 0) {
            return;
        }
        logI << "[LatencyStats] count=" << latency_count_ << ", avg_us=" << (latency_count_ > 0 ? latency_sum_ / latency_count_ : 0.0) << ", min_us=" << (latency_count_ > 0 ? latency_min_ : 0.0)
             << ", max_us=" << latency_max_;
    }

    static constexpr int64_t kStatsDumpInterval{20};

    PidController controller_{PidConfig{}};
    double current_velocity_{5.0};
    std::shared_ptr<const star::setting::planning::TrajectoryOutput> latest_trajectory_;
    std::mutex latency_mutex_;
    int64_t latency_count_{0};
    double latency_sum_{0.0};
    double latency_min_{1e18};
    double latency_max_{0.0};
};

inline auto register_control_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<ControlComponent>("star::control::ControlComponent");
    registry.register_component<ControlComponent>("star::control::Control");
}

}  // namespace star::control

#endif  // __CONTROL_COMPONENT_H__

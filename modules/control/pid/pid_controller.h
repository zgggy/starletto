#ifndef __PID_CONTROLLER_H__
#define __PID_CONTROLLER_H__

#include <cmath>
#include <memory>

#include "modules/setting/schema/control/command.s.h"
#include "modules/setting/schema/planning/trajectory.s.h"

namespace star::control {

struct PidConfig {
    double kp{1.0};
    double ki{0.01};
    double kd{0.05};
    double max_throttle{1.0};
    double max_brake{1.0};
    double max_steering{0.5};
};

class PidController {
  public:
    explicit PidController(PidConfig config) : config_(std::move(config)) {}

    auto compute(const star::setting::planning::TrajectoryOutput& trajectory, double current_velocity) -> star::setting::control::ControlCommand {
        auto cmd = star::setting::control::ControlCommand{};
        cmd.frame_id = trajectory.frame_id;
        cmd.timestamp_ns = trajectory.timestamp_ns;

        if (trajectory.points.empty()) {
            return cmd;
        }

        const auto& target = trajectory.points.front();
        const auto velocity_error = target.velocity - current_velocity;
        integral_ += velocity_error * dt_s_;
        const auto derivative = (velocity_error - prev_error_) / dt_s_;
        prev_error_ = velocity_error;

        auto control = config_.kp * velocity_error + config_.ki * integral_ + config_.kd * derivative;

        if (control > 0) {
            cmd.throttle = std::min(control, config_.max_throttle);
            cmd.brake = 0.0;
        } else {
            cmd.throttle = 0.0;
            cmd.brake = std::min(-control, config_.max_brake);
        }

        cmd.steering = std::clamp(target.curvature * 2.5, -config_.max_steering, config_.max_steering);
        cmd.gear = target.velocity >= 0 ? 1 : -1;

        return cmd;
    }

  private:
    PidConfig config_;
    double integral_{0.0};
    double prev_error_{0.0};
    double dt_s_{0.1};
};

}  // namespace star::control

#endif // __PID_CONTROLLER_H__

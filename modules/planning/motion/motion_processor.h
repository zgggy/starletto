#ifndef __MOTION_PROCESSOR_H__
#define __MOTION_PROCESSOR_H__

#include <cmath>
#include <vector>

#include "modules/setting/schema/planning/trajectory.s.h"

namespace star::planning {

struct MotionInput {
    double vehicle_x{0.0};
    double vehicle_y{0.0};
    double vehicle_heading{0.0};
    double vehicle_velocity{0.0};
};

class MotionProcessor {
  public:
    auto process(const MotionInput& input) -> star::setting::planning::TrajectoryOutput {
        auto output = star::setting::planning::TrajectoryOutput{};
        output.frame_id = ++frame_id_;
        const auto now_ns = static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        output.timestamp_ns = now_ns;

        constexpr auto horizon_s = 2.0;
        constexpr auto dt_s = 0.1;
        constexpr auto num_points = static_cast<int>(horizon_s / dt_s);

        auto x = input.vehicle_x;
        auto y = input.vehicle_y;
        auto heading = input.vehicle_heading;
        auto v = input.vehicle_velocity;

        for (int i = 0; i < num_points; ++i) {
            auto t = (i + 1) * dt_s;
            auto curvature = 0.05 * std::sin(0.5 * t);
            heading += v * curvature * dt_s;
            x += v * std::cos(heading) * dt_s;
            y += v * std::sin(heading) * dt_s;

            auto point = star::setting::planning::TrajectoryPoint{};
            point.x = x;
            point.y = y;
            point.heading = heading;
            point.velocity = v;
            point.curvature = curvature;
            point.relative_time_s = t;
            output.points.emplace_back(std::move(point));
        }
        return output;
    }

  private:
    int64_t frame_id_{0};
};

}  // namespace star::planning

#endif // __MOTION_PROCESSOR_H__

#ifndef __LOCALIZATION_POSE_COMPONENT_H__
#define __LOCALIZATION_POSE_COMPONENT_H__

#include <chrono>
#include <cmath>

#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/setting/schema/localization/pose.s.h"

namespace star::localization {

class PoseComponent final : public star::runtime::Component {
  public:
    auto process_once(const star::runtime::FrameInputs&) -> bool override {
        auto msg = star::localization::pose::Pose{};
        msg.timestamp_ns = now_ns();
        const auto t = static_cast<double>(msg.timestamp_ns) * 1e-9;

        // 假定位：沿小圆匀速运动，便于观察规划/控制跟随
        constexpr auto radius_m = 10.0;
        constexpr auto speed_mps = 5.0;
        const auto omega = speed_mps / radius_m;
        msg.x = radius_m * std::cos(omega * t);
        msg.y = radius_m * std::sin(omega * t);
        msg.z = 0.0;
        msg.heading = std::atan2(std::cos(omega * t), -std::sin(omega * t));
        msg.linear_velocity = speed_mps;
        msg.angular_velocity = omega;
        return write("pose", msg).ok();
    }

  private:
    auto now_ns() const -> int64_t {
        const auto frame = context() ? context()->current_frame().monotonic_time_ns : 0;
        if (frame > 0) {
            return frame;
        }
        return static_cast<int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
    }
};

inline auto register_pose_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<PoseComponent>("star::localization::Pose");
}

}  // namespace star::localization

#endif  // __LOCALIZATION_POSE_COMPONENT_H__

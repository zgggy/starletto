#ifndef __CONTROL_CHASSIS_COMPONENT_H__
#define __CONTROL_CHASSIS_COMPONENT_H__

#include <chrono>
#include <cmath>

#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/setting/schema/control/chassis.s.h"

namespace star::control {

class ChassisComponent final : public star::runtime::Component {
  public:
    auto process_once(const star::runtime::FrameInputs&) -> bool override {
        auto msg = star::control::chassis::Chassis{};
        msg.timestamp_ns = now_ns();
        const auto t = static_cast<double>(msg.timestamp_ns) * 1e-9;

        // 假底盘：速度在小范围波动，档位前进
        msg.speed = 5.0 + 0.5 * std::sin(t);
        msg.steering_angle = 0.02 * std::sin(0.2 * t);
        msg.throttle = 0.3;
        msg.brake = 0.0;
        msg.gear = 1;
        return write("chassis", msg).ok();
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

inline auto register_chassis_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<ChassisComponent>("star::control::ChassisComponent");
}

}  // namespace star::control

#endif  // __CONTROL_CHASSIS_COMPONENT_H__

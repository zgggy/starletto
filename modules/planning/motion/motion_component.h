#ifndef __MOTION_COMPONENT_H__
#define __MOTION_COMPONENT_H__

#include <memory>

#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/planning/motion/motion_processor.h"
#include "modules/setting/schema/control/chassis.s.h"
#include "modules/setting/schema/localization/pose.s.h"
#include "modules/setting/schema/perception/prediction.s.h"
#include "modules/setting/schema/planning/trajectory.s.h"

namespace star::planning {

class MotionComponent : public star::runtime::Component {
  public:
    auto process_once(const star::runtime::FrameInputs& inputs) -> bool override {
        const auto& frame = context()->current_frame();
        if (frame.frame_id > 0) {
            logI << "MotionComponent processing frame=" << frame.frame_id;
        }
        auto input = MotionInput{};
        if (auto pose = trigger_input<star::localization::pose::Pose>(inputs, "localization")) {
            input.vehicle_x = pose->x;
            input.vehicle_y = pose->y;
            input.vehicle_heading = pose->heading;
            input.vehicle_velocity = pose->linear_velocity;
        }
        if (auto chassis = trigger_input<star::control::chassis::Chassis>(inputs, "chassis")) {
            // 底盘反馈优先于定位里的速度
            input.vehicle_velocity = chassis->speed;
        }
        if (auto prediction = trigger_input<star::perception::prediction::Prediction>(inputs, "prediction")) {
            logD << "MotionComponent inputs: predicted_objects=" << prediction->objects.size() << ", v=" << input.vehicle_velocity;
        }
        auto output = processor_.process(input);
        if (not output.points.empty()) {
            logI << "TrajectoryOutput: frame=" << output.frame_id << ", points=" << output.points.size() << ", first=(" << output.points.front().x << "," << output.points.front().y
                 << ") vel=" << output.points.front().velocity;
        }
        write("trajectory", output);
        return true;
    }

  private:
    MotionProcessor processor_;
};

inline auto register_native_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<MotionComponent>("star::planning::Motion");
}

}  // namespace star::planning

#endif  // __MOTION_COMPONENT_H__

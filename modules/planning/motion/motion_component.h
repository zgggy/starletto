#ifndef __MOTION_COMPONENT_H__
#define __MOTION_COMPONENT_H__

#include <memory>

#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/component/typed_component.h"
#include "modules/planning/motion/motion_processor.h"
#include "modules/setting/schema/control/chassis.s.h"
#include "modules/setting/schema/localization/pose.s.h"
#include "modules/setting/schema/perception/prediction.s.h"
#include "modules/setting/schema/planning/trajectory.s.h"

namespace star::planning {

class MotionComponent final
    : public star::runtime::TypedComponent<MotionComponent,
          star::runtime::TriggerInput<star::perception::prediction::Prediction, "prediction">,
          star::runtime::TriggerInput<star::localization::pose::Pose, "localization">,
          star::runtime::TriggerInput<star::control::chassis::Chassis, "chassis">> {
  public:
    auto process_once(const auto& prediction, const auto& pose, const auto& chassis) -> bool {
        const auto& frame = context()->current_frame();
        if (frame.frame_id > 0) {
            logI << "MotionComponent processing frame=" << frame.frame_id;
        }

        auto input = MotionInput{};
        input.vehicle_x = pose.x;
        input.vehicle_y = pose.y;
        input.vehicle_heading = pose.heading;
        input.vehicle_velocity = pose.linear_velocity;
        // 底盘反馈优先于定位里的速度
        input.vehicle_velocity = chassis.speed;
        logD << "MotionComponent inputs: predicted_objects=" << prediction.objects.size() << ", v=" << input.vehicle_velocity;

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

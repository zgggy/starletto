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

struct MotionTriggerInputs {
    std::shared_ptr<const star::perception::prediction::Prediction> prediction;
    std::shared_ptr<const star::localization::pose::Pose> localization;
    std::shared_ptr<const star::control::chassis::Chassis> chassis;
};

class MotionComponent final : public star::runtime::TypedComponent<MotionComponent, MotionTriggerInputs> {
  public:
    using Base = star::runtime::TypedComponent<MotionComponent, MotionTriggerInputs>;
    using Base::process_once;

    auto make_inputs(const star::runtime::FrameInputs& raw) -> MotionTriggerInputs {
        return {
            trigger_input<star::perception::prediction::Prediction>(raw, "prediction"),
            trigger_input<star::localization::pose::Pose>(raw, "localization"),
            trigger_input<star::control::chassis::Chassis>(raw, "chassis"),
        };
    }

    auto process_once(const MotionTriggerInputs& inputs) -> bool {
        const auto& frame = context()->current_frame();
        if (frame.frame_id > 0) {
            logI << "MotionComponent processing frame=" << frame.frame_id;
        }
        auto input = MotionInput{};
        if (inputs.localization) {
            input.vehicle_x = inputs.localization->x;
            input.vehicle_y = inputs.localization->y;
            input.vehicle_heading = inputs.localization->heading;
            input.vehicle_velocity = inputs.localization->linear_velocity;
        }
        if (inputs.chassis) {
            // 底盘反馈优先于定位里的速度
            input.vehicle_velocity = inputs.chassis->speed;
        }
        if (inputs.prediction) {
            logD << "MotionComponent inputs: predicted_objects=" << inputs.prediction->objects.size() << ", v=" << input.vehicle_velocity;
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

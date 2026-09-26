#ifndef __MOTION_COMPONENT_H__
#define __MOTION_COMPONENT_H__

#include <memory>

#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/planning/motion/motion_processor.h"
#include "modules/setting/schema/config/system.s.h"
#include "modules/setting/schema/config_snapshot.s.h"
#include "modules/setting/schema/parameter_snapshot.s.h"
#include "modules/setting/schema/planning/motion.s.h"
#include "modules/setting/schema/planning/trajectory.s.h"

namespace star::planning {

class MotionComponent : public star::runtime::Component {
  public:
    auto process_once() -> bool override {
        const auto& frame = context()->current_frame();
        if (frame.frame_id > 0) {
            logI << "MotionComponent processing frame=" << frame.frame_id;
        }
        maybe_log_first_parameter_snapshot();
        maybe_log_first_config_snapshot();
        auto input = MotionInput{};
        auto output = processor_.process(input);
        if (not output.points.empty()) {
            logI << "TrajectoryOutput: frame=" << output.frame_id << ", points=" << output.points.size() << ", first=(" << output.points.front().x << "," << output.points.front().y
                 << ") vel=" << output.points.front().velocity;
        }
        write("trajectory", output);
        return true;
    }

  private:
    void maybe_log_first_parameter_snapshot() {
        bool should_log = false;
        auto expected_cycle_time = int64_t{0};
        auto trajectory_method = std::string{};
        auto enable_fallback = false;
        {
            const auto snap = read_latest<star::generated::setting::parameter::snapshot::ParameterSnapshot>("parameter_snapshot");
            if (snap && not logged_parameter_snapshot_) {
                logged_parameter_snapshot_ = true;
                should_log = true;
                const auto param = decode_snapshot_entry<star::generated::setting::parameter::snapshot::ParameterSnapshot, star::setting::planning::MotionParameter>(snap, "planning.motion",
                                                                                                                                                                                "planning.motion");
                if (param.has_value()) {
                    expected_cycle_time = param->processor.expected_cycle_time;
                    if (param->methods.has_value()) {
                        trajectory_method = param->methods->trajectory_method;
                        enable_fallback = param->methods->enable_fallback;
                    }
                }
            }
        }
        if (should_log) {
            logI << "MotionComponent received parameter snapshot, expected_cycle_time=" << expected_cycle_time << ", trajectory_method=" << trajectory_method
                 << ", enable_fallback=" << (enable_fallback ? "true" : "false");
        }
    }

    void maybe_log_first_config_snapshot() {
        bool should_log = false;
        auto clock_mode = std::string{};
        auto enable_monitor = false;
        {
            const auto snap = read_latest<star::generated::setting::config::snapshot::ConfigSnapshot>("config_snapshot");
            if (snap && not logged_config_snapshot_) {
                logged_config_snapshot_ = true;
                should_log = true;
                const auto config =
                    decode_snapshot_entry<star::generated::setting::config::snapshot::ConfigSnapshot, star::setting::config::system::SystemConfig>(snap, "system.system", "system.system");
                if (config.has_value()) {
                    clock_mode = config->system.clock_mode;
                    enable_monitor = config->system.enable_monitor;
                }
            }
        }
        if (should_log) {
            logI << "MotionComponent received config snapshot, clock_mode=" << clock_mode << ", enable_monitor=" << (enable_monitor ? "true" : "false");
        }
    }

    MotionProcessor processor_;
    bool logged_parameter_snapshot_{false};
    bool logged_config_snapshot_{false};
};

inline auto register_native_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<MotionComponent>("star::planning::MotionComponent");
    registry.register_component<MotionComponent>("star::planning::Motion");
}

}  // namespace star::planning

#endif  // __MOTION_COMPONENT_H__

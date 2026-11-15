#ifndef __PERCEPTION_PREDICTION_COMPONENT_H__
#define __PERCEPTION_PREDICTION_COMPONENT_H__

#include <chrono>
#include <cmath>

#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/setting/schema/perception/prediction.s.h"

namespace star::perception {

class PredictionComponent final : public star::runtime::Component {
  public:
    auto process_once(const star::runtime::FrameInputs&) -> bool override {
        auto msg = star::perception::prediction::Prediction{};
        msg.timestamp_ns = now_ns();

        // 假预测：前方一个沿车道匀速行驶的目标
        auto object = star::perception::prediction::PredictedObject{};
        object.id = 1;
        object.x = 20.0;
        object.y = 0.0;
        object.heading = 0.0;
        object.velocity = 5.0;
        object.length = 4.5;
        object.width = 1.9;
        msg.objects.push_back(object);
        return write("prediction", msg).ok();
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

inline auto register_prediction_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<PredictionComponent>("star::perception::Prediction");
}

}  // namespace star::perception

#endif  // __PERCEPTION_PREDICTION_COMPONENT_H__

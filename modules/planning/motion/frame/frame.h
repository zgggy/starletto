#ifndef __FRAME_H__
#define __FRAME_H__

/**
 * 注意：本文件是规划数据模型的**骨架**，未接入任何构建目标，当前无法编译。
 *
 * 三个成员（prediction / localization / chassis）引用的是尚未实现的
 * localization / perception 模块的传感器输入类型；项目硬约束为 Native-only，
 * 上述 *_msgs 类型不应进入主树，待这些模块落地时应改为 schema 生成类型。
 *
 * 同目录的 *_context.h 亦无外部引用。整组在 motion 组件真正使用前保持骨架状态。
 */

#include <cstddef>
#include <memory>

#include "libraries/common/blackboard/blackboard.h"
#include "libraries/element/container/decision_info.h"
#include "libraries/element/decision/decision_builder.h"
#include "chassis_msgs/msg/chassis.hpp"
#include "localization_msgs/msg/localization.hpp"
#include "perception_msgs/msg/prediction.hpp"
#include "frame_context.h"
#include "path_context.h"
#include "refline_context.h"
#include "routing_context.h"
#include "traj_context.h"

namespace star::planning::data {

/**
 * Frame: The unified data carrier passed through all planning stages.
 */
class Frame : public star::common::Blackboard {
  public:
    Frame() : decision_builder(planning::DecisionConfig()) {}
    ~Frame() = default;

    auto routing_context() const -> const RoutingContext& { return routing_context_; }
    auto refline_context() const -> const ReflineContext& { return refline_context_; }
    auto traj_context() const -> const TrajContext& { return traj_context_; }
    auto path_context() const -> const PathContext& { return path_context_; }

    auto routing_context() -> RoutingContext& { return routing_context_; }
    auto refline_context() -> ReflineContext& { return refline_context_; }
    auto traj_context() -> TrajContext& { return traj_context_; }
    auto path_context() -> PathContext& { return path_context_; }

    std::shared_ptr<const perception_msgs::msg::Prediction> prediction;
    std::shared_ptr<const localization_msgs::msg::Localization> localization;
    std::shared_ptr<const chassis_msgs::msg::Chassis> chassis;

    RoutingContext routing_context_;
    ReflineContext refline_context_;
    PathContext path_context_;
    TrajContext traj_context_;

    planning::DecisionInfo decision_info;
    planning::DecisionInfo last_decision_info;  // 用于历史一致性
    planning::DecisionBuilder decision_builder;

    FrameContext last_frame_context;
    FrameContext next_frame_context;

    size_t sequence_number = 0;
};

}  // namespace star::planning::data

#endif  // __FRAME_H__

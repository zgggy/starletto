#ifndef __STEP_SLQP_H__
#define __STEP_SLQP_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/unit_node.h"

namespace star::planning::machine {

/**
 * StepSLQP: The step_sl_qp node.
 * Inherits from UnitNode abstract class.
 */
class StepSLQP : public star::node::UnitNode {
  public:
    StepSLQP(const std::string& name) : UnitNode(name) {}

    void init() override {
        // 初始化逻辑
    }

    void enter() override {
        // 进入状态逻辑
    }

    void exit() override {
        // 退出状态逻辑
    }

    void process(std::shared_ptr<star::common::Blackboard> data) override { static_cast<void>(data); }

    bool is_ready() const override { return true; }
};

// Register the StepSLQP class with the NodeFactory
REGISTER_NODE(StepSLQP, UnitNode);

} // namespace star::planning::machine

#endif // __STEP_SLQP_H__

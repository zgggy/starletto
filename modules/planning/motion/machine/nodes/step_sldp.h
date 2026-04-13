#ifndef __STEP_SLDP_H__
#define __STEP_SLDP_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/unit_node.h"

namespace star::planning::machine {

/**
 * StepSLDP: The step_sl_dp node.
 * Inherits from UnitNode abstract class.
 */
class StepSLDP : public star::node::UnitNode {
  public:
    StepSLDP(const std::string& name) : UnitNode(name) {}

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

// Register the StepSLDP class with the NodeFactory
REGISTER_NODE(StepSLDP, UnitNode);

} // namespace star::planning::machine

#endif // __STEP_SLDP_H__

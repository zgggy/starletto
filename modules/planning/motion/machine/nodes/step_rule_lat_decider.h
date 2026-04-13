#ifndef __STEP_RULE_LAT_DECIDER_H__
#define __STEP_RULE_LAT_DECIDER_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/unit_node.h"

namespace star::planning::machine {

/**
 * StepRuleLatDecider: The step_rule_lat_decider node.
 * Inherits from UnitNode abstract class.
 */
class StepRuleLatDecider : public star::node::UnitNode {
  public:
    StepRuleLatDecider(const std::string& name) : UnitNode(name) {}

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

// Register the StepRuleLatDecider class with the NodeFactory
REGISTER_NODE(StepRuleLatDecider, UnitNode);

} // namespace star::planning::machine

#endif // __STEP_RULE_LAT_DECIDER_H__

#ifndef __STEP_DATA_PREP_H__
#define __STEP_DATA_PREP_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/unit_node.h"

namespace star::planning::machine {

/**
 * StepDataPrep: The step_data_prep node.
 * Inherits from UnitNode abstract class.
 */
class StepDataPrep : public star::node::UnitNode {
  public:
    StepDataPrep(const std::string& name) : UnitNode(name) {}

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

// Register the StepDataPrep class with the NodeFactory
REGISTER_NODE(StepDataPrep, UnitNode);

} // namespace star::planning::machine

#endif // __STEP_DATA_PREP_H__

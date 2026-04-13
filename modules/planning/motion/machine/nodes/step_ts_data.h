#ifndef __STEP_TS_DATA_H__
#define __STEP_TS_DATA_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/unit_node.h"

namespace star::planning::machine {

/**
 * StepTSData: The step_ts_data_pre node.
 * Inherits from UnitNode abstract class.
 */
class StepTSData : public star::node::UnitNode {
  public:
    StepTSData(const std::string& name) : UnitNode(name) {}

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

// Register the StepTSData class with the NodeFactory
REGISTER_NODE(StepTSData, UnitNode);

} // namespace star::planning::machine

#endif // __STEP_TS_DATA_H__

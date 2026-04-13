#ifndef __PHASE_SL_H__
#define __PHASE_SL_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/group_node.h"

namespace star::planning::machine {

/**
 * PhaseSL: The phase_sl node.
 * Inherits from GroupNode abstract class.
 */
class PhaseSL : public star::node::GroupNode {
  public:
    PhaseSL(const std::string& name) : GroupNode(name) {}

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

// Register the PhaseSL class with the NodeFactory
REGISTER_NODE(PhaseSL, GroupNode);

} // namespace star::planning::machine

#endif // __PHASE_SL_H__

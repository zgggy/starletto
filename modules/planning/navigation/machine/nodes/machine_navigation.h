#ifndef __MACHINE_NAVIGATION_H__
#define __MACHINE_NAVIGATION_H__

#include "machine/builder/node_factory.h"
#include "libraries/common/blackboard/blackboard.h"
#include "machine/node/group_node.h"

namespace star::planning::machine {

/**
 * MachineNavigation: The machine_navigation node.
 * Inherits from GroupNode abstract class.
 */
class MachineNavigation : public star::node::GroupNode {
  public:
    MachineNavigation(const std::string& name) : GroupNode(name) {}

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

// Register the MachineNavigation class with the NodeFactory
REGISTER_NODE(MachineNavigation, GroupNode);

} // namespace star::planning::machine

#endif // __MACHINE_NAVIGATION_H__

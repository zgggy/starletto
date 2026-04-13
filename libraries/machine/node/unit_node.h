#ifndef __UNIT_NODE_H__
#define __UNIT_NODE_H__

#include "base_node.h"

namespace star::node {

class UnitNode : public BaseNode {
  public:
    UnitNode(const std::string& name) : BaseNode(name) {}
    void organize_childs([[maybe_unused]] const std::vector<std::shared_ptr<BaseNode>>& children) override {}
    auto pick_child() -> std::shared_ptr<BaseNode> final { return nullptr; }
};

}  // namespace star::node

#endif  // __UNIT_NODE_H__
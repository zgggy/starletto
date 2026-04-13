#ifndef __PICK_NODE_H__
#define __PICK_NODE_H__

#include <algorithm>
#include <vector>

#include "base_node.h"

namespace star::node {

/**
 * PickNode: If and only if the is_ready function of a child node returns true,
 *           enter this child node without any other condition.
 *           To inherit from the PickNode, users need to implement the is_ready function and the process function on their own.
 */
class PickNode : public BaseNode {
  public:
    PickNode(const std::string& name) : BaseNode(name) {}

    void organize_childs(const std::vector<std::shared_ptr<BaseNode>>& children) override {
        // 添加子节点并设置父节点
        for (const auto& child : children) {
            add_child(child);
            child->set_father(shared_from_this());
        }
        // 设置第一个子节点为当前子节点
        if (not children.empty()) {
            set_cur_child(children[0]);
        }
    }

    auto pick_child() -> std::shared_ptr<BaseNode> final {
        auto ready_childs = std::vector<std::shared_ptr<BaseNode>> {};
        for (const auto& [child_name, child] : childs()) {
            if (child->is_ready()) {
                ready_childs.emplace_back(child);
            }
        }
        if (ready_childs.size() > 1) {
            throw excep::RuntimeError("Node [", name_, "]", ready_childs.size(), " childs ready");
        } else if (ready_childs.size() < 1) {
            return nullptr;
        } else {
            return ready_childs.at(0);
        }
    }
};

}  // namespace star::node

#endif  // __PICK_NODE_H__
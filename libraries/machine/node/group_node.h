#ifndef __GROUP_NODE_H__
#define __GROUP_NODE_H__

#include <algorithm>
#include <vector>

#include "base_node.h"
namespace star::node {

/**
 * GroupNode: node group together, all childs run in sequence in one loop.
 */
class GroupNode : public BaseNode {
  protected:
    std::vector<std::shared_ptr<BaseNode>> child_sequence_ {};

  public:
    GroupNode(const std::string& name) : BaseNode(name) {}

    void organize_childs(const std::vector<std::shared_ptr<BaseNode>>& children) override {
        child_sequence_.clear();
        for (const auto& child : children) {
            add_child(child);
            child->set_father(shared_from_this());
            child_sequence_.push_back(child);
        }
        if (not children.empty()) {
            set_cur_child(children[0]);
        }
    }

    auto pick_child() -> std::shared_ptr<BaseNode> final { return nullptr; }

    auto process_once(std::shared_ptr<star::base::Blackboard> data_stream) -> bool override {
        for (const auto& child : child_sequence_) {
            child->enter();
            if (not child->process_once(data_stream)) {
                return false;
            }
            child->exit();
            if (child->status() == ERROR) {
                set_status(ERROR);
                return false;
            }
        }
        set_status(DONE);
        return true;
    }
};

} // namespace star::node

#endif // __GROUP_NODE_H__

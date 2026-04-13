#ifndef __SEQ_NODE_H__
#define __SEQ_NODE_H__

#include <algorithm>
#include <map>
#include <vector>

#include "base_node.h"

namespace star::node {

/**
 * SeqNode: When the status of a child node changes to Done, enter the next child node.
 *          child nodes allow duplicates in the child node list.
 *          If the current child node is the last one in the child list, stay in this child node.
 *          The process function of the child node should correctly set its running status.
 *          To inherit from the SeqNode, users need to implement the process function on their own.
 */
class SeqNode : public BaseNode {
  protected:
    int current_index_ {0};
    std::vector<std::shared_ptr<BaseNode>> child_sequence_;

  public:
    SeqNode(const std::string& name) : BaseNode(name) {}

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

    auto pick_child() -> std::shared_ptr<BaseNode> final {
        if (child_sequence_.empty()) return nullptr;
        auto current_child = child_sequence_[std::min(current_index_, static_cast<int>(child_sequence_.size()) - 1)];
        if (current_child->status() == UNKNOWN) {
            return current_child;
        } else if (current_child->status() == PROCESSING) {
            return current_child;
        } else if (current_child->status() == DONE) {
            current_child->set_status(UNKNOWN);
            current_index_++;
            if (current_index_ < static_cast<int>(child_sequence_.size())) {
                return child_sequence_[current_index_];
            } else {
                return child_sequence_.back();
            }
        } else if (current_child->status() == ERROR) {
            return nullptr;
        } else {
            return current_child;
        }
    }
};

} // namespace star::node

#endif // __SEQ_NODE_H__
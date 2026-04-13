#ifndef __FSM_NODE_H__
#define __FSM_NODE_H__

#include "base_node.h"

namespace star::node {

/**
 * FsmNode: Build a finite state machine.
 *          children transfer to each other according to the transition table
 *          while the condition of transition is satisfied.
 */
class FsmNode : public BaseNode {
  protected:
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::function<bool()>>>> childs_transitions_;

  public:
    FsmNode(const std::string& name) : BaseNode(name) {}

    void organize_childs(const std::vector<std::shared_ptr<BaseNode>>& children) override {
        for (const auto& child : children) {
            add_child(child);
            child->set_father(shared_from_this());
        }
        if (not children.empty()) {
            set_cur_child(children[0]);
        }
        childs_transitions_.clear();
        for (const auto& from_node : children) {
            std::vector<std::pair<std::string, std::function<bool()>>> transitions;
            for (const auto& to_node : children) {
                if (from_node->name() != to_node->name()) {
                    // 绑定check_transition方法
                    auto transition_func = std::bind(&FsmNode::check_transition, this, from_node->name(), to_node->name());
                    transitions.emplace_back(to_node->name(), transition_func);
                }
            }
            childs_transitions_[from_node->name()] = transitions;
        }
    }

    auto pick_child() -> std::shared_ptr<BaseNode> final {
        auto it = childs_transitions_.find(cur_child_->name());
        if (it != childs_transitions_.end()) {
            for (const auto& [to_name, transition_func] : it->second) {
                if (transition_func()) {
                    return child(to_name);
                }
            }
        }
        return cur_child_;
    }

    virtual auto check_transition(const std::string& from, const std::string& to) -> bool {
        return false;
    }
};

} // namespace star::node

#endif // __FSM_NODE_H__

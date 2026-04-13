#ifndef __BASE_NODE_H__
#define __BASE_NODE_H__

#include "basement/core/exception.h"           // namsp excep
#include "libraries/common/blackboard/blackboard.h"  // namsp common

#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace star::node {

enum NodeStatus : uint8_t { UNKNOWN = 0u, PROCESSING = 1u, DONE = 2u, ERROR = 3u };

/**
 * BaseNode: Base class for all nodes.
 */
class BaseNode : public std::enable_shared_from_this<BaseNode> {
  protected:
    std::string name_;
    NodeStatus status_;
    std::weak_ptr<BaseNode> father_;
    std::unordered_map<std::string, std::shared_ptr<BaseNode>> childs_;
    std::shared_ptr<BaseNode> cur_child_;
    star::common::Blackboard variables_;

  public:
    BaseNode(const std::string& name) : name_(name) {}
    virtual ~BaseNode() = default;
    virtual void organize_childs(const std::vector<std::shared_ptr<BaseNode>>& children) = 0;
    virtual auto is_ready() const -> bool = 0;
    virtual void init() = 0;
    virtual void enter() = 0;
    virtual void exit() = 0;
    virtual void process(std::shared_ptr<star::common::Blackboard>) = 0;
    virtual auto pick_child() -> std::shared_ptr<BaseNode> = 0;

    /**
     * Pass the data from the upper-level node or the external environment to the process function,
     * and then pass the output of the process function to the process_once function of the current child node.
     */
    void enter_recursive() {
        enter();
        if (cur_child_) {
            cur_child_->enter_recursive();
        }
    }
    void exit_recursive() {
        if (cur_child_) {
            cur_child_->exit_recursive();
        }
        exit();
    }
    void reset_recursive() {
        if (cur_child_) {
            cur_child_->reset_recursive();
        }
        init();
        set_status(UNKNOWN);
    }

    virtual auto process_once(std::shared_ptr<star::common::Blackboard> data_stream) -> bool {
        process(data_stream);
        if (childs_.empty()) {
            return true;
        }
        auto picked_child = pick_child();
        if (picked_child == nullptr) {
            return false;
        }
        if (cur_child_  and  cur_child_ != picked_child) {
            cur_child_->exit_recursive();
            cur_child_->reset_recursive();
            cur_child_ = picked_child;
            cur_child_->enter_recursive();
        }
        return cur_child_->process_once(data_stream);
    }

  public:
    auto name() const -> std::string { return name_; }
    auto status() const -> NodeStatus { return status_; }
    auto father() const -> std::weak_ptr<BaseNode> { return father_.lock(); }
    auto top_father() const -> std::shared_ptr<BaseNode> {
        auto current = std::const_pointer_cast<BaseNode>(shared_from_this());
        while (auto next = current->father().lock()) {
            current = next;
        }
        return current;
    }

    auto childs() const -> std::unordered_map<std::string, std::shared_ptr<BaseNode>> { return childs_; }
    auto child(const std::string& name) const -> std::shared_ptr<BaseNode> {
        auto iter = childs_.find(name);
        if (iter == childs_.end()) {
            return nullptr;
        }
        return iter->second;
    }
    auto cur_child() const -> std::shared_ptr<BaseNode> { return cur_child_; }

    auto any_node(const std::string& name) const -> std::shared_ptr<BaseNode> { return top_father()->descendant(name); }
    auto descendant(const std::string& name) const -> std::shared_ptr<BaseNode> {
        if (name_ == name) {
            return std::const_pointer_cast<BaseNode>(shared_from_this());
        }
        for (const auto& pair : childs()) {
            auto child = pair.second;
            auto found = child->descendant(name);
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    template <typename _AnyType>
    auto var(std::string name) const -> _AnyType {
        return variables_.get<_AnyType>(name);
    }

    auto set_status(NodeStatus status) -> NodeStatus {
        status_ = status;
        return status_;
    }

    void set_father(std::shared_ptr<BaseNode> father) { father_ = father; }
    void add_child(std::shared_ptr<BaseNode> child) {
        if (not child) {
            throw excep::RuntimeError("Node [", name_, "]: child node cannot be null");
        }
        auto iter = childs_.find(child->name());
        if (iter != childs_.end()) {
            throw excep::RuntimeError("Node [", name_, "] has already a child: ", child->name());
        }
        childs_.insert_or_assign(child->name(), child);
    }
    void set_cur_child(std::shared_ptr<BaseNode> cur_child) { cur_child_ = cur_child; }
    void set_cur_child(const std::string& cur_child_name) { set_cur_child(child(cur_child_name)); }
    template <typename _T>
    void add_var(std::string name, const _T& data) { variables_.add(name, data); }
    template <typename _T>
    void set_var(std::string name, const _T& data) { variables_.set(name, data); }
};

}  // namespace star::node

#endif  // __BASE_NODE_H__

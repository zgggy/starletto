#ifndef __NODE_FACTORY_H__
#define __NODE_FACTORY_H__

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "../node/base_node.h"

namespace star::node {

class NodeFactory {
  public:
    static std::shared_ptr<BaseNode> create_node(const std::string& node_name, const std::string& node_type) {
        if (node_name.empty()) {
            throw std::invalid_argument("Node name cannot be empty");
        }
        if (node_type.empty()) {
            throw std::invalid_argument("Node type cannot be empty");
        }
        auto it = creators_.find(node_type);
        if (it != creators_.end()) {
            return it->second(node_name);
        }
        throw std::invalid_argument("Unknown node type: " + node_type);
    }

    static void register_node(const std::string& node_type, const std::function<std::shared_ptr<BaseNode>(const std::string&)>& creator) {
        creators_[node_type] = creator;
    }

  private:
    using NodeCreator = std::function<std::shared_ptr<BaseNode>(const std::string&)>;

    static inline std::unordered_map<std::string, NodeCreator> creators_ = []() { return std::unordered_map<std::string, NodeCreator>(); }();
};

} // namespace star::node

// Macro for registering node classes
#define REGISTER_NODE(NODE_TYPE, ABSTRACT_TYPE)                                                                                           \
    extern "C" {                                                                                                                          \
    struct NodeRegistrar_##NODE_TYPE {                                                                                                    \
        NodeRegistrar_##NODE_TYPE() {                                                                                                     \
            star::node::NodeFactory::register_node(#NODE_TYPE, [](const std::string& name) { return std::make_shared<NODE_TYPE>(name); }); \
        }                                                                                                                                 \
    };                                                                                                                                    \
    static NodeRegistrar_##NODE_TYPE g_node_registrar_##NODE_TYPE;                                                                        \
    }

#endif // __NODE_FACTORY_H__
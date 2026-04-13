#ifndef __MACHINE_BUILDER_H__
#define __MACHINE_BUILDER_H__

#include "node_factory.h"
#include "node_structure_parser.h"

#include "basement/core/logger/logger.h"

#include <fstream>
#include <memory>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

namespace star::node {

class MachineBuilder {
  public:
    static std::shared_ptr<BaseNode> build_from_file(const std::string& file_path) {
        if (file_path.empty()) {
            throw std::runtime_error("Structure file path cannot be empty");
        }

        // Parse the structure file using NodeStructureParser
        auto root_structure = NodeStructureParser::parse_file(file_path);

        // Create node tree from structure
        return create_node_from_structure(root_structure);
    }

  private:
    static std::shared_ptr<BaseNode> create_node_from_structure(const std::shared_ptr<NodeStructure>& structure) {
        if (not structure) {
            throw std::runtime_error("MachineBuilder: null node structure");
        }
        logD << "Creating node: " << structure->name << " type: " << structure->type;
        auto node = NodeFactory::create_node(structure->name, structure->type);

        std::vector<std::shared_ptr<BaseNode>> children;
        for (const auto& child_structure : structure->children) {
            auto child_node = create_node_from_structure(child_structure);
            children.push_back(child_node);
        }

        node->init();
        node->organize_childs(children);

        return node;
    }
};

} // namespace star::node

#endif // __MACHINE_BUILDER_H__

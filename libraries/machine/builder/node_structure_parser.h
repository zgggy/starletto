#ifndef __NODE_STRUCTURE_PARSER_H__
#define __NODE_STRUCTURE_PARSER_H__

#include <fstream>
#include <cctype>
#include <memory>
#include <regex>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "../node/base_node.h"

namespace star::node {

struct NodeStructure {
    std::string name;
    std::string type;
    std::string base_type;
    std::vector<std::shared_ptr<NodeStructure>> children;
    std::unordered_set<std::string> child_names;

    NodeStructure(const std::string& name, const std::string& type, const std::string& base_type) : name(name), type(type), base_type(base_type) {}
};

class NodeStructureParser {
  public:
    static auto parse_file(const std::string& file_path) -> std::shared_ptr<NodeStructure> {
        std::ifstream file(file_path);
        if (not file.is_open()) {
            throw std::runtime_error("Failed to open structure file: " + file_path);
        }

        std::vector<std::string> lines;
        std::string line;
        size_t line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            std::string original_line = line;
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            std::string content = trim(line);
            if (not content.empty()) {
                lines.push_back(std::to_string(line_num) + ":" + content + ":" + original_line);
            }
        }
        file.close();

        if (lines.empty()) {
            throw std::runtime_error("Empty structure content in file: " + file_path);
        }

        return parse_lines(lines);
    }

  private:
    static auto trim(const std::string& str) -> std::string {
        size_t begin = 0;
        while (begin < str.size()  and  std::isspace(static_cast<unsigned char>(str[begin]))) {
            begin++;
        }
        if (begin == str.size()) {
            return "";
        }
        size_t end = str.size() - 1;
        while (end > begin  and  std::isspace(static_cast<unsigned char>(str[end]))) {
            end--;
        }
        return str.substr(begin, end - begin + 1);
    }

    static auto is_valid_identifier(const std::string& identifier) -> bool {
        if (identifier.empty()) return false;
        for (char c : identifier) {
            if (not isalnum(c)  and  c != '_') return false;
        }
        return true;
    }

    static auto parse_lines(const std::vector<std::string>& packed_lines) -> std::shared_ptr<NodeStructure> {
        std::shared_ptr<NodeStructure> root = nullptr;
        std::stack<std::shared_ptr<NodeStructure>> node_stack;

        for (const auto& packed : packed_lines) {
            size_t first_colon = packed.find(':');
            size_t second_colon = packed.find(':', first_colon + 1);
            
            std::string line_num_str = packed.substr(0, first_colon);
            std::string line = packed.substr(first_colon + 1, second_colon - first_colon - 1);
            std::string original_line = packed.substr(second_colon + 1);

            if (line == "}") {
                if (node_stack.empty()) {
                    throw std::runtime_error("Unexpected closing brace '}' at line " + line_num_str + "\n  > " + original_line);
                }
                node_stack.pop();
                continue;
            }

            bool has_brace = false;
            std::string content = line;
            if (not line.empty()  and  line.back() == '{') {
                has_brace = true;
                content = line.substr(0, line.size() - 1);
                content = trim(content);
            }

            std::vector<std::string> parts = split(content);
            if (parts.size() < 2) {
                throw std::runtime_error("Invalid node definition at line " + line_num_str + ": Node must have 'name' and 'type'\n  > " + original_line);
            }

            std::string node_name = parts[0];
            std::string node_type = parts[1];
            std::string base_type = (parts.size() > 2) ? parts[2] : (has_brace ? "Group" : "Unit");

            if (not is_valid_identifier(node_name)) {
                throw std::runtime_error("Invalid node name at line " + line_num_str + ": '" + node_name + "' (only alphanumeric and underscores allowed)\n  > " + original_line);
            }
            if (not is_valid_identifier(node_type)) {
                throw std::runtime_error("Invalid node type at line " + line_num_str + ": '" + node_type + "'\n  > " + original_line);
            }

            std::string node_base_type;
            if (base_type == "Fsm") {
                node_base_type = "FsmNode";
            } else if (base_type == "Group") {
                node_base_type = "GroupNode";
            } else if (base_type == "Pick") {
                node_base_type = "PickNode";
            } else if (base_type == "Seq") {
                node_base_type = "SeqNode";
            } else {
                node_base_type = "UnitNode";
            }

            auto node = std::make_shared<NodeStructure>(node_name, node_type, node_base_type);

            if (node_stack.empty()) {
                if (root != nullptr) {
                    throw std::runtime_error("Multiple root nodes found at line " + line_num_str + ". Only one top-level node is allowed.\n  > " + original_line);
                }
                root = node;
            } else {
                auto& parent = node_stack.top();
                if (parent->child_names.count(node_name)) {
                    throw std::runtime_error("Duplicate node name '" + node_name + "' in container '" + parent->name + "' at line " + line_num_str + "\n  > " + original_line);
                }
                parent->children.push_back(node);
                parent->child_names.insert(node_name);
            }

            if (has_brace) {
                node_stack.push(node);
            } else if (not has_brace  and  node_stack.empty()) {
                // If it's a leaf node and not in a stack, it means it's a root leaf node which is not allowed.
                // Except if it's the very first line and only one line, but normally the root should be a container.
                // Let's enforce that the root must be a container or we'll have issues.
                // Actually, the Python script allows it but it's better to have a container root.
            }
        }

        if (not node_stack.empty()) {
            throw std::runtime_error("Container node '" + node_stack.top()->name + "' was never closed with '}'");
        }

        return root;
    }

    static auto split(const std::string& str) -> std::vector<std::string> {
        std::vector<std::string> result;
        std::string current;
        for (char c : str) {
            if (c == ' '  or  c == '\t') {
                if (not current.empty()) {
                    result.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (not current.empty()) {
            result.push_back(current);
        }
        return result;
    }
};

} // namespace star::node

#endif // __NODE_STRUCTURE_PARSER_H__

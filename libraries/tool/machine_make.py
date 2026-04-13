import os
import shutil
import re
import sys
import subprocess

def camel_to_snake(name):
    s1 = re.sub("(.)([A-Z][a-z]+)", r"\1_\2", name)
    return re.sub("([a-z0-9])([A-Z])", r"\1_\2", s1).lower()

class Node:
    def __init__(self, name, node_type, base_class_short="Final", parent_path=""):
        self.name = name
        self.type = node_type
        base_map = {
            "Unit": "UnitNode",
            "Fsm": "FsmNode",
            "Group": "GroupNode",
            "Seq": "SeqNode",
            "Pick": "PickNode"
        }
        self.base_class = base_map.get(base_class_short, base_class_short)
        self.children = []
        self.parent_path = parent_path

    @property
    def is_container(self):
        return self.base_class in ["FsmNode", "GroupNode", "SeqNode", "PickNode"]

    @property
    def rel_path(self):
        snake_name = camel_to_snake(self.name)
        if not self.parent_path:
            return snake_name
        return os.path.join(self.parent_path, snake_name)

class StructureParser:
    def __init__(self, file_path):
        self.file_path = file_path
        self.errors = []
        self.all_types = set()

    def _add_error(self, line_num, message, line_content=""):
        err = f"Error at line {line_num}: {message}"
        if line_content:
            err += f"\n  > {line_content.strip()}"
        self.errors.append(err)

    def _is_valid_identifier(self, identifier):
        return re.match(r"^[a-zA-Z0-9_]+$", identifier) is not None

    def validate_syntax(self):
        if not os.path.exists(self.file_path):
            return False

        with open(self.file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

        stack = [] # Element: (name, line_num, child_names_set)
        root_found = False
        
        for line_num, original_line in enumerate(lines, 1):
            comment_split = original_line.split("#", 1)
            line = comment_split[0].strip()
            
            if not line:
                continue

            if line.endswith("{"):
                content = line.rstrip("{").strip()
                parts = content.split()
                
                if len(parts) < 2:
                    self._add_error(line_num, "Container node must have 'name' and 'type' before '{'", original_line)
                    continue
                
                name, node_type = parts[0], parts[1]
                
                if not self._is_valid_identifier(name):
                    self._add_error(line_num, f"Invalid node name: '{name}' (only alphanumeric and underscores allowed)", original_line)
                if not self._is_valid_identifier(node_type):
                    self._add_error(line_num, f"Invalid node type: '{node_type}'", original_line)
                
                # Check for duplicate name in current scope (same parent)
                if stack:
                    parent_name, parent_ln, children_names = stack[-1]
                    if name in children_names:
                        self._add_error(line_num, f"Duplicate node name '{name}' in container '{parent_name}'", original_line)
                    children_names.add(name)

                self.all_types.add(node_type)

                if not stack:
                    if root_found:
                        self._add_error(line_num, "Multiple root nodes found. Only one top-level node is allowed.", original_line)
                    root_found = True
                
                stack.append((name, line_num, set()))
                
            elif line == "}":
                if not stack:
                    self._add_error(line_num, "Unexpected closing brace '}' without matching '{'", original_line)
                else:
                    stack.pop()
            else:
                parts = line.split()
                if len(parts) < 2:
                    self._add_error(line_num, "Leaf node must have 'name' and 'type'", original_line)
                    continue
                
                name, node_type = parts[0], parts[1]
                
                if not self._is_valid_identifier(name):
                    self._add_error(line_num, f"Invalid node name: '{name}'", original_line)
                if not self._is_valid_identifier(node_type):
                    self._add_error(line_num, f"Invalid node type: '{node_type}'", original_line)

                # Check for duplicate name in current scope (same parent)
                if not stack:
                    self._add_error(line_num, f"Leaf node '{name}' found outside of any container", original_line)
                else:
                    parent_name, parent_ln, children_names = stack[-1]
                    if name in children_names:
                        self._add_error(line_num, f"Duplicate node name '{name}' in container '{parent_name}'", original_line)
                    children_names.add(name)

                self.all_types.add(node_type)

        while stack:
            name, line_num, _ = stack.pop()
            self._add_error(line_num, f"Container node '{name}' was never closed with '}}'")

        if self.errors:
            print("\n" + "="*50)
            print("SYNTAX ERRORS FOUND IN STRUCTURE FILE:")
            print("="*50)
            for err in self.errors:
                print(err)
            print("="*50 + "\n")
            return False
            
        return True

    def format_file(self):
        if not os.path.exists(self.file_path):
            return

        with open(self.file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

        parsed_data = []
        for line in lines:
            original_line = line.rstrip()
            comment = ""
            if "#" in original_line:
                original_line, comment = original_line.split("#", 1)
                comment = "#" + comment

            content = original_line.strip()
            if not content:
                parsed_data.append({"type": "empty", "comment": comment, "indent": 0})
                continue

            indent = len(original_line) - len(original_line.lstrip())
            
            if content == "}":
                parsed_data.append({"type": "brace_close", "indent": indent, "comment": comment})
                continue

            is_open = content.endswith("{")
            if is_open:
                content = content.rstrip("{").strip()
            
            parts = content.split()
            name = parts[0] if len(parts) > 0 else ""
            node_type = parts[1] if len(parts) > 1 else ""
            base = parts[2] if len(parts) > 2 else ""
            
            parsed_data.append({
                "type": "node",
                "indent": indent,
                "name": name,
                "node_type": node_type,
                "base": base,
                "is_open": is_open,
                "comment": comment
            })

        for i in range(len(parsed_data)):
            if parsed_data[i]["type"] == "empty" and parsed_data[i]["comment"]:
                for j in range(i + 1, len(parsed_data)):
                    if parsed_data[j]["type"] != "empty":
                        parsed_data[i]["indent"] = parsed_data[j]["indent"]
                        break

        def format_group(group):
            if not group: return []
            max_name_len = max(len(item["name"]) + item["indent"] for item in group)
            max_type_len = max(len(item["node_type"]) for item in group)
            
            res = []
            for item in group:
                indent_str = " " * item["indent"]
                line = (indent_str + item["name"]).ljust(max_name_len + 1)
                line += item["node_type"].ljust(max_type_len + 1)
                if item["base"]:
                    line += item["base"]
                if item["is_open"]:
                    line = line.rstrip() + " {"
                if item["comment"]:
                    line = line.ljust(max_name_len + max_type_len + 4) + item["comment"]
                res.append(line.rstrip())
            return res

        formatted_lines = []
        current_group = []
        current_indent = -1

        for item in parsed_data:
            if item["type"] == "node":
                if current_indent == -1:
                    current_indent = item["indent"]
                    current_group.append(item)
                elif item["indent"] == current_indent:
                    current_group.append(item)
                else:
                    formatted_lines.extend(format_group(current_group))
                    current_group = [item]
                    current_indent = item["indent"]
            else:
                if current_group:
                    formatted_lines.extend(format_group(current_group))
                    current_group = []
                    current_indent = -1
                
                if item["type"] == "empty":
                    indent_str = " " * item["indent"]
                    formatted_lines.append(indent_str + item["comment"] if item["comment"] else "")
                elif item["type"] == "brace_close":
                    line = " " * item["indent"] + "}"
                    if item["comment"]:
                        line = line.ljust(30) + item["comment"]
                    formatted_lines.append(line.rstrip())

        if current_group:
            formatted_lines.extend(format_group(current_group))

        with open(self.file_path, "w", encoding="utf-8") as f:
            f.write("\n".join(formatted_lines) + "\n")

    def parse(self):
        if not os.path.exists(self.file_path):
            raise FileNotFoundError(f"Structure file not found: {self.file_path}")

        with open(self.file_path, "r", encoding="utf-8") as f:
            lines = f.readlines()

        root = None
        stack = []

        for line_num, line in enumerate(lines, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue

            if line.endswith("{"):
                parts = line.rstrip("{").strip().split()
                name, node_type = parts[0], parts[1]
                base = parts[2] if len(parts) > 2 else "Group"
                
                parent_path = stack[-1].rel_path if stack else ""
                node = Node(name, node_type, base, parent_path)
                if stack:
                    stack[-1].children.append(node)
                else:
                    root = node
                stack.append(node)
            elif line == "}":
                if stack:
                    stack.pop()
            else:
                parts = line.split()
                name, node_type = parts[0], parts[1]
                base = parts[2] if len(parts) > 2 else "Unit"
                
                parent_path = stack[-1].rel_path if stack else ""
                node = Node(name, node_type, base, parent_path)
                if stack:
                    stack[-1].children.append(node)

        return root

class CodeEmitter:
    def __init__(self, root_node, nodes_dir):
        self.root = root_node
        self.nodes_dir = nodes_dir
        self.all_nodes = []
        self._collect_all_nodes(self.root)

    def _collect_all_nodes(self, node):
        self.all_nodes.append(node)
        for child in node.children:
            self._collect_all_nodes(child)

    def _to_upper_snake(self, name):
        s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
        return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).upper()

    def emit_header_files(self):
        if not os.path.exists(self.nodes_dir):
            os.makedirs(self.nodes_dir)

        existing_files = set()
        for root, dirs, files in os.walk(self.nodes_dir):
            for f in files:
                if f.endswith(".h"):
                    full_path = os.path.join(root, f)
                    rel_to_nodes = os.path.relpath(full_path, self.nodes_dir)
                    existing_files.add(rel_to_nodes)

        active_files = set()
        emitted_types = set()

        for node in self.all_nodes:
            # Flatten based on TYPE (class name)
            file_rel_path = f"{camel_to_snake(node.type)}.h"
            active_files.add(file_rel_path)
            
            # If we've already emitted a header for this TYPE, skip
            if node.type in emitted_types:
                continue
            emitted_types.add(node.type)
            
            file_path = os.path.join(self.nodes_dir, file_rel_path)
            
            # Since we're flattening, nodes_dir is the parent, and we don't need subdirs
            # But we'll keep the os.makedirs just in case nodes_dir itself needs to be created
            os.makedirs(os.path.dirname(file_path), exist_ok=True)

            if os.path.exists(file_path):
                continue

            print(f"Generating header for {node.type} -> {file_rel_path}")
            
            guard_name = f"__{self._to_upper_snake(node.type)}_H__"
            
            base_snake = camel_to_snake(node.base_class)
            content = [
                f"#ifndef {guard_name}",
                f"#define {guard_name}",
                "",
                '#include "machine/builder/node_factory.h"',
                '#include "basement/old/io/blackboard/blackboard.h"',
                f'#include "machine/node/{base_snake}.h"',
                "",
                "namespace star::planning::machine {",
                "",
                f"/**",
                f" * {node.type}: The {node.name} node.",
                f" * Inherits from {node.base_class} abstract class.",
                f" */",
                f"class {node.type} : public star::node::{node.base_class} {{",
                "  public:",
                f"    {node.type}(const std::string& name) : {node.base_class}(name) {{}}",
                "",
                "    void init() override {",
                "        // 初始化逻辑",
                "    }",
                "",
                "    void enter() override {",
                "        // 进入状态逻辑",
                "    }",
                "",
                "    void exit() override {",
                "        // 退出状态逻辑",
                "    }",
                "",
                "    void process(std::shared_ptr<star::base::Blackboard> data) override { static_cast<void>(data); }",
                "",
                "    bool is_ready() const override { return true; }",
            ]

            if node.base_class == "FsmNode":
                content.extend([
                    "",
                    "    auto check_transition(const std::string& from, const std::string& to) -> bool {",
                    "        static_cast<void>(from);",
                    "        static_cast<void>(to);",
                    "        // 实现具体的跳转逻辑",
                    "        return false;",
                    "    }"
                ])

            content.extend([
                "};",
                "",
                f"// Register the {node.type} class with the NodeFactory",
                f"REGISTER_NODE({node.type}, {node.base_class});",
                "",
                "} // namespace star::planning::machine",
                "",
                f"#endif // {guard_name}"
            ])
            
            with open(file_path, "w", encoding="utf-8") as f:
                f.write("\n".join(content))

        for f_rel in existing_files:
            if f_rel not in active_files and not os.path.basename(f_rel).startswith("_"):
                old_path = os.path.join(self.nodes_dir, f_rel)
                dir_name = os.path.dirname(old_path)
                base_name = os.path.basename(old_path)
                new_path = os.path.join(dir_name, f"_{base_name}")
                print(f"Renaming obsolete header file: {f_rel} -> _{base_name}")
                os.rename(old_path, new_path)

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 machine_make.py <machine_file_path> <nodes_dir_path>")
        sys.exit(1)

    config_path = os.path.abspath(sys.argv[1])
    nodes_dir = os.path.abspath(sys.argv[2])
    
    # Auto-detect machine.tree if structure.txt is passed but doesn't exist
    if os.path.basename(config_path) == "structure.txt" and not os.path.exists(config_path):
        tree_path = os.path.join(os.path.dirname(config_path), "machine.tree")
        if os.path.exists(tree_path):
            config_path = tree_path
            print(f"Auto-switching to: {config_path}")
    
    print(f"Checking structure syntax: {config_path}")
    parser = StructureParser(config_path)
    if not parser.validate_syntax():
        sys.exit(1)
    
    print(f"Formatting structure file: {config_path}")
    parser.format_file()
    
    print(f"Parsing structure: {config_path}")
    root = parser.parse()

    if not root:
        print("Error: No root node found.")
        return

    emitter = CodeEmitter(root, nodes_dir)
    print(f"Emitting header files in: {nodes_dir}")
    emitter.emit_header_files()
    header_make_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "header_make.py")
    subprocess.run([sys.executable, header_make_path, nodes_dir], check=True)
    print("Done!")

if __name__ == "__main__":
    main()

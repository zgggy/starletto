#!/usr/bin/env python3
import os
import sys

def main():
    """
    递归遍历指定 nodes 目录下的所有头文件，生成 machine_decl.h 聚合头。
    用法:
        python3 header_make.py <nodes_dir> [<output_file>]
    说明:
        - <nodes_dir>    必填，指向 .../machine/nodes 目录
        - <output_file>  选填，默认写到 <nodes_dir>/../machine_decl.h
        - 会自动忽略以下划线开头的头文件（视为过时）
        - 生成的 include 形如 "<module>/<rel_path_from_module_dir>"
    """
    if len(sys.argv) < 2:
        print("Usage: python3 header_make.py <nodes_dir> [<output_file>]")
        sys.exit(1)

    nodes_dir = os.path.abspath(sys.argv[1])
    machine_dir = os.path.dirname(nodes_dir)
    module_dir = os.path.dirname(machine_dir)
    module_name = os.path.basename(module_dir)
    output_file = os.path.abspath(sys.argv[2]) if len(sys.argv) >= 3 else os.path.join(machine_dir, "machine_decl.h")

    if not os.path.exists(nodes_dir):
        print(f"Error: nodes directory not found at {nodes_dir}")
        sys.exit(1)

    # 收集头文件（忽略以 '_' 开头的过时头）
    header_files = []
    for root, _, files in os.walk(nodes_dir):
        for file in files:
            if file.endswith(".h") and not file.startswith("_"):
                full_path = os.path.join(root, file)
                rel_to_module = os.path.relpath(full_path, module_dir)  # e.g., "machine/nodes/xxx.h"
                header_files.append(rel_to_module)

    content = [
        "#ifndef __MACHINE_DECL_H__",
        "#define __MACHINE_DECL_H__",
        "",
        "// 自动生成的文件，包含所有节点头文件",
        "// 由 header_make.py 脚本生成",
        "",
    ]

    # 统一以 <module>/ 前缀包含，保持与工程包含路径一致
    for header in sorted(header_files):
        content.append(f'#include "{module_name}/{header}"')

    content.extend([
        "",
        "namespace star::planning::machine {",
        "",
        "inline void force_register_node() {",
        "    // 空函数，仅用于强制编译器包含所有节点头文件，从而确保静态注册变量被初始化",
        "}",
        "",
        "} // namespace star::planning::machine",
        "",
        "#endif // __MACHINE_DECL_H__",
    ])

    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("\n".join(content))

    print(f"Generated machine_decl.h at {output_file}")
    print(f"Included {len(header_files)} header files")

if __name__ == "__main__":
    main()

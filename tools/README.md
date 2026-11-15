# tools —— 开发/CI 辅助

| 路径 | 作用 |
|---|---|
| `new_component/scaffold.py` | `star new module/component`：生成骨架并接好 conf/meson/静态注册 |
| `tests/codegen_selftest.py` | codegen 自检（reserved/多 root/import/哈希） |
| `tests/scaffold_selftest.py` | 脚手架结构与三处接线自检 |
| `tests/oot_plugin_smoke.sh` | 安装树 + 源码树外插件 + 静态 runner 冒烟 |
| `install_surface/prune_include_tree.py` | 安装 include 树时删除非头文件 |

这些脚本由 `star check` 调用，也单独可跑。

# 技术栈选型

## 语言与标准

| 项目 | 选择 | 版本 |
|------|------|------|
| 语言 | C++ | C++20 |
| 标准 | Meson + Ninja | — |
| 包管理 | 无（源码依赖） | — |

## 核心依赖

| 依赖 | 用途 | 版本要求 |
|------|------|----------|
| GCC / Clang | 编译器 | C++20 支持 |
| Python 3 | codegen 脚本 | 3.8+ |
| Meson | 构建系统 | 0.60+ |
| Ninja | 构建后端 | 1.10+ |
| dlfcn.h | 动态库加载 | POSIX |
| sys/wait.h | 进程管理 | POSIX |

## 禁止引入的依赖

- ROS2 相关任何包
- protobuf / flatbuffers
- spdlog / glog / 其他日志库
- boost（除非核心功能必须）
- 任何需要复杂构建的外部库

**关联决策**：[20260411_tech.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/03_decisions/20260411_tech.md)

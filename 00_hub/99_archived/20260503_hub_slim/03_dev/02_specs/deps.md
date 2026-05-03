# 依赖管理

## 外部依赖

本项目设计为 **最小外部依赖**：

### 必需依赖（系统级）

| 依赖 | 用途 | 引入方式 |
|------|------|----------|
| libc | POSIX API (fork/exec/wait/dlfcn/shm) | 系统自带 |
| libstdc++ | C++ 标准库 | 编译器自带 |
| librt | SHM 时钟 | `-lrt`（Meson 自动处理） |
| libdl | dlopen/dlsym | `-ldl`（Meson 自动处理） |
| pthread | 线程/互斥 | 系统自带 |

### 可选依赖

| 依赖 | 用途 | 引入方式 |
|------|------|----------|
| Python 3 | schema_codegen.py | 构建时需要 |
| Catch2 / Googletest | 单元测试（B5 阶段引入） | 待决定 |

## 内部依赖关系

```
core ← schema ← transport ← runtime ← manifest
                              ↑
                           tools (CLI)
```

- core：零内部依赖
- schema：仅依赖 core（exception/logger）
- transport：依赖 core + schema
- runtime：依赖 core + schema + transport + manifest
- manifest：依赖 core + schema
- tools：依赖全部 basement 模块

## 禁止引入

- ROS2 任何包
- Boost（除非核心功能必须且无替代方案）
- 第三方日志库（已有自研 LoggerManager）
- 第三方序列化库（已有自研 binary_codec）

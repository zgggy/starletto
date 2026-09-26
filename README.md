# Starletto

## 介绍

Starletto 是一个自动驾驶开放平台，核心是一套 **Native-first** 的自研运行时底座：统一类型体系（Schema）、零拷贝通信（Transport）、可预测的组件运行时（Runtime），以及配套的构建/诊断/录制回放工具链。

平台对标 Apollo CyberRT（强类型 + 零拷贝 + 工具闭环）、Autoware（模块边界与组件实践）与 ROS2（生态可扩展性）的能力子集，但**主线不依赖 ROS2**：主路径完全自研，与 ROS2 的 bridge 需作为独立工程接入。

## 项目组成

```
00_hub/docs/ 项目文档（spec / 架构 / 评审 / 发布记录）
basement/    运行时底座（自底向上）
             ├── core      日志 / 异常 / 时钟 / 类型擦除指针
             ├── schema    .schema IDL + codegen + 二进制 codec + 兼容检查
             ├── transport inproc 指针直传 / shm 零拷贝 / routed 路由 / record-replay
             └── runtime   组件模型 / 调度 / 插件 / 进程编排 / 健康与观测
libraries/   算法与通用库（禁止依赖 transport/runtime）
             element（领域类型）/ method（规划算法）/ machine（行为树）/ common / tool
modules/     业务组件（setting / planning / control / debug）
apps/        宿主程序与工具（native_runner 及各 demo/工具）
data/        标定与地图数据
```

分层约束：`libraries` 只依赖 `basement/core` 与必要的数学/几何库；`runtime` 不定义业务消息结构；`transport` 不关心业务含义。

## 技术栈

- **C++20**：主要开发语言，主体为 header-only，统一 `.clang-format` 风格
- **Meson / Ninja**：构建系统（`star` 脚本是统一入口）
- **Python3**：schema 代码生成与工具链（构建期自动执行）
- **POSIX**：`dl`（插件动态加载）、`rt` + shm/mmap/futex（跨进程零拷贝）
- **vendored 第三方**：`nlohmann/json`、`toml++`（单头，位于 `libraries/third_party/`）
- **OSQP**：仅 `libraries/method/algorithm/piecewise_jerk/` 使用，尚未接入构建目标

> 说明：不依赖 ROS2、colcon 或 Eigen。`libraries/element` 的领域类型（Frenet、ST 图、ReferenceLine 等）与 `libraries/method` 的规划算法（A* / Hybrid A* / piecewise jerk）为纯 C++ 实现。

## 快速开始

### 环境要求

- C++20 编译器（Clang / GCC）
- Meson >= 1.0 与 Ninja
- Python3（schema codegen）

### 构建与运行

```bash
# 构建（内部执行 meson setup + meson compile）
./star build

# 重新配置 / 指定 build 目录
./star build --reconfigure
./star build --build-dir=build

# 运行 native runner（单进程装配 module.conf / component.conf）
./star run planning

# 多进程编排（按 process_group 划分，跨进程走 shm）
./star run --multi-process <module.conf>

# 安装与清理
./star install          # 安装产物到本地目录（DESTDIR 方式）
./star clean            # 清理 Meson build 目录
```

### 诊断与观测

```bash
./star list                     # 列出模块与组件
./star graph --components       # 输出组件/Topic 拓扑图（dot）
./star doctor --deep            # 诊断清单，校验插件可 dlopen/dlsym
./star monitor                  # 实时 transport 统计（频率/延迟/丢帧/QoS）
./star selftest                 # transport 自检（如 routed 不重复投递）
./star check                    # CI Guard：build + doctor + 冒烟验证
```

### 数据与工具

```bash
./star param list               # typed parameter service（list/get/set, --format=json）
./star schema gen|fmt|compat    # schema 工具（代码生成/格式化/兼容性检查）
./star record info <文件>       # 查看录制文件（消息数/topics/时间范围）
./star replay <文件> [speed]    # 按时间戳回放，支持速率控制
./star test                     # 运行单元测试（Meson test）
```

`star help` 可查看完整命令列表。

## 许可证

MIT License

# 项目定位

## 我们是谁

Starletto 是一个 **Native-only 自动驾驶运行时系统**，从零构建，不依赖 ROS2。

## 市场位置

| 维度 | Starletto | Apollo CyberRT | Autoware | ROS2 |
|------|-----------|----------------|----------|------|
| 定位 | 轻量运行时底座 | 完整自动驾驶平台 | 组件化 AD 栈 | 通用机器人中间件 |
| 通信 | inproc 指针 + shm 零拷贝 | shm + 零拷贝 | DDS/ROS2 topic | DDS |
| 类型体系 | .schema IDL + codegen | protobuf | msg/ROS interface | msg/IDL |
| 调度 | choreography + priority | choreography + dag | 简单调度 | executor |
| 语言 | C++20 | C++ | C++/Python | 多语言 |
| 目标用户 | 自研自动驾驶团队 | 百度生态 | Autoware 社区 | 机器人开发者 |

## 核心差异化

1. **Schema-first**：所有类型统一走 `.schema` IDL，codegen 自动生成 C++ View/Builder
2. **Zero-copy 强约束**：单进程指针传递、跨进程 shm loan/view
3. **Native-only**：不引入 ROS 依赖，保持最小依赖树
4. **Spec-driven 开发**：每个模块有独立 spec 文档，开发前先写契约
5. **三工程架构**：basement（底座）/ libraries（算法）/ modules（业务）独立演进

## 当前阶段

**Phase 1：Basement 底座完善（~93%）**

- Schema 100% 完成
- Transport 95% 完成（monitor/p99/supervisor 已就绪）
- Runtime 88% 完成（health checker 已就绪）
- Core 95% 完成（rolling logger / unified format / iclock 已就绪）

下一步：测试基础设施 + M4 系统闭环

# Starletto

## 介绍

Starletto 是一个功能强大的自动驾驶开放平台，基于 ROS2 构建，提供完整的自动驾驶算法框架和开发工具链。

## 项目组成

Starletto 项目分为以下几大模块：

### Libraries 库
公共库目录，包含底层支撑代码、算法库和通用工具。原则上核心算法代码不直接依赖 ROS2；与中间件交互的实现应收敛在边界层（例如 basement/runtime 或 modules）。

### Messages 消息
ROS2 消息定义目录，包含各子系统的消息接口定义。

### Modules 模块
模块目录，包含所有 ROS2 业务节点实现。

### Documents 文档
项目文档，包含代码规范、设计文档等。

## 技术栈

- **C++17/20**: 主要开发语言
- **ROS2**: 机器人中间件
- **colcon**: 构建系统
- **Eigen**: 线性代数库
- **OSQP**: 优化求解器

## 快速开始

### 环境要求
- C++ 编译器 (支持 C++17/20)
- ROS2 (rolling 或 humble)
- colcon

### 构建项目

```bash
# 构建整个项目
./star build

# 指定并行线程数
./star build -j 8

# Release 模式
./star build -R
```

### 设置环境

```bash
source artifacts/install/setup.bash
```

## 构建命令

```bash
# 构建
./star build

# 清理
./star clean

# 测试
./star test <模块名>
```

## 许可证

MIT License

# Starletto 系统手册

> **版本**: v0.2.9 | **日期**: 2026-04-11 | **Basement 成熟度**: ~99%
>
> 本文档面向三类读者：**使用者 / 二次开发者 / 维护者**。按需阅读对应章节。

---

## 目录

- [1. 一句话介绍](#1-一句话介绍)
- [2. 系统定位与设计哲学](#2-系统定位与设计哲学)
- [3. 工程结构总览](#3-工程结构总览)
- [4. 核心概念词典](#4-核心概念词典)
  - [4.1 Component（组件）](#41-component组件)
  - [4.2 Channel（通道）](#42-channel通道)
  - [4.3 ServiceBus（服务总线）](#43-servicebus服务总线)
  - [4.4 EventBus（事件总线）](#44-eventbus事件总线)
  - [4.5 Scheduler（调度器）](#45-scheduler调度器)
  - [4.6 Trigger（触发器）](#46-trigger触发器)
  - [4.7 Manifest（清单）](#47-manifest清单)
  - [4.8 Schema（类型系统）](#48-schema类型系统)
  - [4.9 MessageBus（消息总线）](#49-messagebus消息总线)
  - [4.10 RuntimeContext（运行时上下文）](#10-runtimecontext运行时上下文)
- [5. 架构详解](#5-架构详解)
  - [5.1 三层工程关系](#51-三层工程关系)
  - [5.2 数据流全景](#52-数据流全景)
  - [5.3 组件生命周期](#53-组件生命周期)
  - [5.4 通信模式选择指南](#54-通信模式选择指南)
  - [5.5 进程模型](#55-进程模型)
- [6. 快速上手](#6-快速上手)
  - [6.1 构建系统](#61-构建系统)
  - [6.2 运行第一个组件](#62-运行第一个组件)
  - [6.3 常用命令速查](#63-常用命令速查)
- [7. 模块能力清单](#7-模块能力清单)
  - [7.1 Core — 基础设施](#71-core--基础设施)
  - [7.2 Schema — 类型系统](#72-schema--类型系统)
  - [7.3 Transport — 传输层](#73-transport--传输层)
  - [7.4 Runtime — 运行时](#74-runtime--运行时)
  - [7.5 Manifest — 清单系统](#75-manifest--清单系统)
  - [7.6 Observability — 可观测性](#76-observability--可观测性)
- [8. 二次开发指南](#8-二次开发指南)
  - [8.1 编写新组件](#81-编写新组件)
  - [8.2 定义新的 Schema 类型](#82-定义新的-schema-类型)
  - [8.3 扩展 Transport 层](#83-扩展-transport-层)
  - [8.4 代码规范摘要](#84-代码规范摘要)
- [9. 维护者参考](#9-维护者参考)
  - [9.1 文件组织约定](#91-文件组织约定)
  - [9.2 构建系统内部](#92-构建系统内部)
  - [9.3 CI Guard](#93-ci-guard)
  - [9.4 版本历史](#94-版本历史)
- [A. 概念映射表（Apollo / Autoware / ROS2）](#a-概念映射表)

---

## 1. 一句话介绍

Starletto 是一个 **Native-only 自动驾驶运行时系统**，提供组件化生命周期管理、零拷贝进程内/跨进程通信、确定性调度、类型安全的 Schema 系统，以及完整的工具链（构建/诊断/监控/录制回放）。

**目标对齐**：Apollo CyberRT 的性能 + Autoware 的组件化 + ROS2 的工具生态（通过 bridge，非依赖）。

---

## 2. 系统定位与设计哲学

### 2.1 核心原则

| 原则 | 说明 |
|------|------|
| **Native-first** | 不依赖 ROS2/CyberRT，从零构建运行时 |
| **Zero-copy** | 进程内指针传递；跨进程 SHM loan/view |
| **Manifest-driven** | `module.conf` + `component.conf` 驱动装配，不硬编码 |
| **Schema-typed** | 所有数据有 Schema 定义，codegen 自动生成类型 |
| **Header-only C++** | 所有代码在 .h 中，Meson 构建 |

### 2.2 设计取舍

| 选择 | 理由 |
|------|------|
| 不用 ROS2 | 消除中间件开销和依赖链 |
| 不用 Iceoryx | 自研 SHM channel，完全可控 |
| 不用协程 | 同步 dispatch + futex 更简单、更确定 |
| 头文件优先 | 减少编译单元链接复杂度，加快增量编译 |
| ConfValue 过渡 | manifest 解析先走通用值树，后续迁移到 schema typed |

---

## 3. 工程结构总览

```
starletto/                          # 工程根目录
├── basement/                      # ⭐ 运行时底座 (~13,500 行, ~99% 成熟)
│   ├── core/                     # 异常 / 日志 / 基础类型
│   ├── schema/                   # IDL / codegen / codec / registry
│   ├── transport/                # 消息总线 / 服务总线 / SHM / 录放
│   ├── runtime/                  # 组件模型 / 调度 / 触发 / 进程编排
│   └── manifest/                 # 清单解析与类型生成
├── libraries/                    # 通用算法库 (配合测试)
├── modules/                      # 业务模块 (planning/control/setting/...)
├── apps/native_runner/           # 入口程序
├── 00_hub/                       # ⭐ 项目管理与文档中心 (docsify 站点)
│   ├── INIT.md                   # 规则中枢
│   ├── index.html                # Docsify 入口
│   ├── _sidebar.md               # 导航栏
│   ├── vendor/                   # Docsify 资源
│   ├── 01_strategy/              # 战略层
│   ├── 02_tasks/                 # 任务层
│   ├── 03_dev/                   # 研发层
│   │   └── 05_docs/              # ← 技术规格 + 本文档
│   └── 04_assets/                # 共享资产
└── star                          # CLI 工具 (build/run/list/doctor/monitor/check)
```

---

## 4. 核心概念词典

### 4.1 Component（组件）

**定义**：系统的基本计算单元，封装独立的功能逻辑。

```
Component
├── set_context(RuntimeContext*)    // 注入运行时环境
├── set_load_plan(ComponentLoadPlan) // 注入装配信息
├── auto_configure()               // 自动配置（读 conf → 设置参数）
├── init()                         // 初始化资源
├── process_once() -> bool         // 单帧处理（核心方法）
└── shutdown()                     // 清理资源
```

**关键约束**：
- 组件不知道自己在哪个进程、哪个调度器上运行
- 组件间只通过 Topic（Channel）或 Service 通信
- 组件可以动态加载（`.so` plugin）或静态链接

**实现位置**：[`component.h`](basement/runtime/component/component.h)

### 4.2 Channel（通道）

**定义**：P2P 点对点通信管道，1 个 Writer → N 个 Reader。

```
Writer.publish(topic, message)  →  [MessageBus]  →  Reader callback(message)
```

**三种传输后端**：

| 后端 | 适用场景 | 零拷贝？ |
|------|---------|---------|
| inproc (`NativeMessageBus`) | 同进程内组件间 | ✅ 指针传递 |
| SHM (`ShmMessageBus`) | 跨进程组件间 | ✅ loan/view |
| Routed (`RoutedMessageBus`) | 自动路由（同组 inproc / 跨组 shm） | ✅ 自动选择 |

**QoS 能力**：
- `DropPolicy`: KEEP_LAST（保留最新）/ DROP_NEWEST（丢弃新消息）
- `depth`: per-reader ring buffer 深度
- p99/p999 延迟统计

### 4.3 ServiceBus（服务总线）

**定义**：Request-Response 模式的同步/异步调用。

```
Client.call(request)  →  [ServiceBus]  →  Server.handler(request) → response
```

**两种实现**：

| 实现 | 传输层 | 场景 |
|------|--------|------|
| `NativeServiceBus` | inproc dispatch | 单进程内服务调用 |
| `ShmServiceBus` | SHM topic 对 (`svc.request` / `svc.response`) | **跨进程服务调用** (F-4) |

**关键机制**：
- `CorrelationId`: 匹配请求与响应
- `promise/future`: Client 端异步等待（10s 超时）
- `Handler`: Server 端回调函数

### 4.4 EventBus（事件总线）

**定义**：1:N 广播式事件通知，fire-and-forget。

```
EventBus.subscribe("system.shutdown", callback)
EventBus.publish("system.shutdown", message)  →  所有 subscriber 收到
```

**与 Channel 的区别**：

| 维度 | Channel (MessageBus) | EventBus |
|------|---------------------|----------|
| 模式 | P2P（1 Writer → N Reader） | **1:N 广播** |
| QoS | ring buffer + depth + DropPolicy | 无缓冲 |
| 典型场景 | 组件间数据流 | 系统级事件通知 |

### 4.5 Scheduler（调度器）

**定义**：管理组件的执行节拍。

```
Scheduler
├── configure(SchedulerConfig)     // 配置 worker 线程数
├── register_task(TaskDescriptor)    // 注册任务（名称/周期/优先级/callback）
├── start()                         // 启动 worker 线程池
├── stop()                          // 停止
└── notify(task_name, frame_id)     // 外部触发任务执行
```

**当前实现**：`NativeScheduler`
- 优先级队列（`std::priority_queue`）
- Choreography 模式支持（多任务编排）
- Worker 线程池可配置

**触发方式**：

| 触发器 | 说明 |
|--------|------|
| `TimerTrigger` | 定时触发（cycle_ms 间隔） |
| `SyncTrigger` | 多 topic 就绪后才触发（同步屏障） |
| topic 数据到达 | Reader 回调中调用 `scheduler.notify()` |

### 4.6 Trigger（触发器）

| 类型 | 用法 | 场景 |
|------|------|------|
| TimerTrigger | `trigger: { type: "timer", cycle_ms: 20 }` | 固定频率组件（规划 50Hz） |
| SyncTrigger | `trigger: { type: "sync_topic", readers: ["camera", "lidar"] }` | 需要多源数据就绪后才开始计算 |

### 4.7 Manifest（清单）

**定义**：声明式配置文件，驱动组件装配。

```yaml
# module.conf
name: planning
components:
  - class: star::planning::MotionPlanner
    conf: modules/planning/motion/component.conf
    executor_threads: 2
    trigger:
      type: timer
      cycle_ms: 20
    outputs:
      - name: trajectory
        topic: planning.trajectory
        record: true
    inputs:
      - name: vehicle_state
        topic: localization.pose
```

**层级**：

| 文件 | 内容 |
|------|------|
| `module.conf` | 模块级：包含哪些组件、进程分组 |
| `component.conf` | 组件级：输入输出端点、trigger、线程数 |

**解析流程**：`module.conf` → `ModuleManifest` → `ComponentLoadPlan` → `NativeComponentHost.load(plan)`

### 4.8 Schema（类型系统）

**定义**：IDL（接口定义语言），描述数据的类型结构。

```schema
schema Trajectory {
    version = 1
    hash = 0xabc123
    root TrajectoryPoint[] points

    def TrajectoryPoint {
        1: Vec3D position
        2: double heading
        3: double speed
        4: double relative_time
    }

    def Vec3D {
        1: double x
        2: double y
        3: double z
    }
}
```

**Codegen 流程**：`.schema` → `schema_codegen.py` → `.s.h`（C++ 类型定义）
- 16 个 builtin schema 已注册（component/module/load_plan/process）
- 运行时可通过 `SchemaRegistry` 动态加载

### 4.9 MessageBus（消息总线）

**抽象接口**（所有传输后端的共同契约）：

```cpp
class MessageBus {
    virtual void declare_topic(const TopicDescriptor&) = 0;
    virtual auto create_writer(const TopicDescriptor&) -> std::shared_ptr<Writer> = 0;
    virtual auto create_reader(const TopicDescriptor&, Reader::Callback) -> std::shared_ptr<Reader> = 0;
    virtual auto dump_stats_lines() const -> std::vector<std::string> = 0;
};
```

**实现类**：

| 类 | 后端 | 位置 |
|----|------|------|
| NativeMessageBus | inproc 同步 dispatch | `transport/native/` |
| ShmMessageBus | SHM loan/view | `transport/shm/` |
| RoutedMessageBus | 自动路由（组合上面两个） | `transport/shm/` |

### 4.10 RuntimeContext（运行时上下文）

**定义**：注入到每个组件的环境句柄。

```cpp
RuntimeContext {
    message_bus()      // → MessageBus (inproc 或 routed)
    parameter_store()  // → ParameterStore (内存 K/V)
    scheduler()        // → Scheduler
    clock()            // → IClock (steady/system/simulation)
    schema_registry()  // → SchemaRegistry
    channel_registry() // → ChannelDescriptor 注册表
}
```

**创建方式**：`RuntimeFactory::create_native_context(transport_name)`

---

## 5. 架构详解

### 5.1 三层工程关系

```
┌─────────────────────────────────────────────┐
│              modules/ (业务)                 │
│  planning / control / setting / debug       │
│  ↓ 使用 basement 提供的 Component 基类       │
│  ↓ 通过 manifest 装配到 runtime             │
├─────────────────────────────────────────────┤
│              libraries/ (算法)                │
│  math / common / perception / ...           │
│  ↓ 纯算法，无中间件依赖                    │
├─────────────────────────────────────────────┤
│              basement/ (底座) ⭐             │
│  core / schema / transport / runtime / manifest │
│  ↓ 提供全部运行时能力                        │
└─────────────────────────────────────────────┘
```

**当前聚焦**：basement。libraries 和 modules 仅用于配合测试。

### 5.2 数据流全景

#### 单进程模式

```
[Component A] --publish(topic_a)--> [MessageBus] --callback--> [Component B]
                                    |
                              [Component C] --callback-->

[Scheduler] --notify(task_b)--> [Component B.process_once()]
```

#### 多进程模式

```
进程 1 (group: perception)          进程 2 (group: planning)
┌─────────────────────┐           ┌─────────────────────┐
│ [Camera Component]  │           │ [Planner Component] │
│     │               │  SHM      │     ↑               │
│     publish(cam)    │ ───────>  │     read(lidar)     │
│     │               │           │     │               │
│     read(trajectory)│ <───────  │     publish(traj)   │
└─────────────────────┘           └─────────────────────┘
         ↕ RoutedMessageBus (自动选择 inproc/SHM) ↕
```

### 5.3 组件生命周期

```
1. load(plan)          → DlHandle(.so) + ComponentRegistry.create()
2. set_context(ctx)    → 注入 RuntimeContext
3. set_load_plan(plan) → 注入装配信息
4. auto_configure()    → 读 component.conf → 设置参数
5. init()              → 分配资源（打开 SHM、注册 Service 等）
6. start()             → Scheduler 启动 / Trigger 激活
7. [循环] process_once() → 每帧业务逻辑
8. stop()              → Scheduler 停止
9. shutdown()          → 释放资源
```

### 5.4 通信模式选择指南

| 场景 | 推荐模式 | 理由 |
|------|---------|------|
| 组件 A 需要持续接收 B 的数据流 | **Channel (P2P)** | QoS 控制 + 统计 |
| 组件需要调用另一个的服务并等结果 | **ServiceBus** | request/response 语义 |
| 系统级事件（启动/停止/错误广播） | **EventBus** | 1:N 广播，不需要确认 |
| 跨进程高频传感器数据 | **Channel via SHM** | 零拷贝 loan/view |
| 跨进程远程过程调用 | **ShmServiceBus** | promise/future 异步 |
| 参数读写 | **ParameterStore** | 内存 K/V，全局共享 |

### 5.5 进程模型

**单进程**（默认）：所有组件在一个进程中，inproc 通信

**多进程**：`ProcessOrchestrator` fork/exec 子进程
- 每个 `ProcessPlan.group` 对应一个子进程
- 组间自动使用 SHM 通信
- 组内使用 inproc 通信
- 主进程负责监控和协调

---

## 6. 快速上手

### 6.1 构建系统

```bash
# 完整构建
./star build

# 仅编译（跳过 codegen 检查）
./star build --skip-check

# CI 自动验证（build + doctor + list）
./star check
```

**构建系统**：Meson + Ninja（入口统一为 `star` CLI）

**前置依赖**：
- g++ (C++17+)
- meson >= 0.60
- ninja
- Python 3（用于 codegen）

### 6.2 运行第一个组件

```bash
# 查看 available 模块
./star list planning

# 诊断模块健康状态
./star doctor planning

# 运行 planning 模块（3 帧，每帧间隔 100ms）
./star run --frames=3 --sleep-ms=100 planning

# 多进程模式
./star run --multi-process planning

# 实时查看 transport 统计
./star monitor planning

# 录制 topic 数据
./star run --record planning
./star record_replay_tool replay --file=/tmp/starletto_record.bin
```

### 6.3 常用命令速查

| 命令 | 功能 |
|------|------|
| `./star build` | 全量编译 |
| `./star check` | CI 验证（build + doctor + list） |
| `./star run <target>` | 运行模块 |
| `./star run --frames=N --sleep-ms=M <target>` | 运行 N 帧 |
| `./star run --multi-process <target>` | 多进程模式 |
| `./star list <target>` | 列出组件 |
| `./star graph <target>` | 输出 DAG 图 |
| `./star doctor <target>` | 诊断检查 |
| `./star monitor <target>` | 实时统计面板 |
| `./star record_replay_tool` | 录制/回放工具 |

---

## 7. 模块能力清单

### 7.1 Core — 基础设施

| 能力 | 文件 | 状态 |
|------|------|------|
| RuntimeError (variadic template) | `core/exception.h` | ✅ |
| logD/logI/logW/logE (零开销宏) | `core/logger/logger.h` | ✅ for-loop 短路模式 |
| RollingFileLogger (按大小滚动) | `core/logger/rolling_file_logger.h` | ✅ |
| FileLogger (install-tree 落盘) | `core/logger/file_logger.h` | ✅ |
| TypedConstPtr / TypedPtr | `core/common_types.h` | ✅ |
| IClock (steady/system/simulation) | `runtime/context/runtime_context.h` | ✅ |

### 7.2 Schema — 类型系统

| 能力 | 文件 | 状态 |
|------|------|------|
| IDL 语法定义 (.schema) | `schema/*.schema` | ✅ 4 builtin |
| Python Codegen | `schema/schema_codegen.py` | ✅ gen/fmt/compat/lint |
| AST 类型定义 | `schema/schema_ast.h` | ✅ |
| Binary Codec (wire format) | `schema/binary_codec.h` | ✅ 559 行功能单体 |
| Schema Registry (运行时) | `schema/schema_registry.h` | ✅ |
| Native Loader (.schema + .conf) | `native/native_schema_loader.h` | ✅ 已拆分 (380行) |
| Syntax Parser (递归下降) | `native/schema_syntax_parser.h` | ✅ 新增 |
| Builtin Schema 注册 (16个) | `native/schema_bootstrap.h` | ✅ |

### 7.3 Transport — 传输层

| 能力 | 文件 | 状态 |
|------|------|------|
| inproc 同步 dispatch | `native/native_message_bus.h` | ✅ 已拆分 (250行) |
| QoS ring buffer + DropPolicy | `native/` + `shm/` | ✅ |
| p99/p999 延迟统计 | native + shm | ✅ compute_percentile() |
| Async Reader (poll_loop + futex) | `shm/shm_reader.h` (嵌套) | ✅ |
| SHM zero-copy loan/view | `shm/shm_channel.h` | ✅ 已拆分 (296行) |
| ShmTextView / ShmBinaryView | `shm/shm_views.h` | ✅ 已拆分 (47行) |
| Auto-route (同组 inproc / 跨组 shm) | `shm/shm_routed_message_bus.h` | ✅ |
| ServiceBus (inproc) | `api/service_bus.h` + `native/` | ✅ |
| **ServiceBus (跨进程 SHM)** | `shm/shm_service_bus.h` | ✅ **F-4 新增** |
| **EventBus (1:N 发布订阅)** | `api/event_bus.h` | ✅ **F-3 新增** |
| ParameterStore (内存) | `native/native_parameter_store.h` | ✅ |
| ChannelRegistry (writer/reader/group) | `api/channel_registry.h` | ✅ |
| STARREC 录制/回放 | `record/message_recorder.h` + `replayer.h` | ✅ |
| dump_stats_lines() (含 dropped/p99/p999) | native + shm | ✅ |

### 7.4 Runtime — 运行时

| 能力 | 文件 | 状态 |
|------|------|------|
| Component 基类 (完整生命周期) | `runtime/component/component.h` | ✅ 418行功能单体 |
| ComponentRegistry (plugin 注册) | `runtime/component/component_registry.h` | ✅ star_register_components |
| Dynamic Library 加载 | `runtime/component/component_dl_helper.h` | ✅ O-5 拆出 (213行) |
| NativeComponentHost (装配+生命周期) | `runtime/component/native_component_host.h` | ✅ 已拆分 (451行) |
| Scheduler (抽象 + Native) | `runtime/scheduler/` | ✅ choreography + priority queue |
| TimerTrigger / SyncTrigger | `runtime/trigger/trigger.h` | ✅ |
| ProcessOrchestrator (fork/exec/waitpid) | `runtime/process/process_orchestrator.h` | ✅ |
| Path Resolver (8 个路径查找函数) | `runtime/process/path_resolver.h` | ✅ |
| RuntimeFactory (上下文工厂) | `runtime/factory/runtime_factory.h` | ✅ |
| FrameContext (frame_id/time_ns) | `runtime/context/frame_context.h` | ✅ |
| **ResourceMonitor (CPU%/RSS/FD/线程)** | `observability/resource_monitor.h` | ✅ **F-1 新增** |
| **HealthChecker (三级severity+资源阈值)** | `observability/health_checker.h` | ✅ **F-2 新增** |
| ChannelStats (频率/payload/延迟/dropped) | `observability/channel_stats.h` | ✅ |

### 7.5 Manifest — 清单系统

| 能力 | 文件 | 状态 |
|------|------|------|
| ComponentManifest 结构体 | `manifest/component_manifest.h` | ✅ |
| ModuleManifest 结构体 | `manifest/module_manifest.h` | ✅ |
| ManifestLoader 抽象接口 | `manifest/manifest_loader.h` | ✅ |
| NativeManifestLoader (ConfValue→typed) | `manifest/native/native_manifest_loader.h` | ✅ |
| NativeRuntimePlanBuilder (manifest→plan) | `manifest/native/native_runtime_plan_builder.h` | ✅ |

### 7.6 Observability — 可观测性

| 能力 | 文件 | 状态 |
|------|------|------|
| ChannelStats (topic 维度统计) | `observability/channel_stats.h` | ✅ |
| ResourceSnapshot (进程维度采样) | `observability/resource_monitor.h` | ✅ F-1 |
| HealthReport (组件维度聚合) | `observability/health_checker.h` | ✅ F-2 |
| Monitor CLI (`star monitor`) | `apps/native_runner/app.h` | ✅ 含 resource 行 |
| CI Guard (`star check`) | `star` 脚本 | ✅ TD-4 |

---

## 8. 二次开发指南

### 8.1 编写新组件

**步骤 1：定义组件类**

```cpp
// my_component.h
#ifndef __MY_COMPONENT_H__
#define __MY_COMPONENT_H__

#include "basement/runtime/component/component.h"

namespace myapp {

class MyComponent final : public star::runtime::Component {
  public:
    MyComponent() = default;

    void auto_configure() override {
        // 从 context.parameter_store() 读取配置
    }

    void init() override {
        // 创建 writer / reader / service client
        trajectory_writer_ = context().message_bus().create_writer({"/my/trajectory", "", {}});
        pose_reader_ = context().message_bus().create_reader(
            {"/localization/pose", "", {}},
            [this](const star::transport::MessageView& msg) { on_pose(msg); });
    }

    bool process_once() override {
        // 业务逻辑
        if (pose_received_) {
            star::transport::MessageView traj_msg;
            traj_msg.schema_type_id = /* your type id */;
            trajectory_writer_->publish("/my/trajectory", traj_msg);
        }
        return true;
    }

    void shutdown() override {
        trajectory_writer_.reset();
        pose_reader_.reset();
    }

  private:
    std::shared_ptr<star::transport::Writer> trajectory_writer_;
    std::shared_ptr<star::transport::Reader> pose_reader_;
    bool pose_received_{false};

    void on_pose(const star::transport::MessageView& msg) {
        pose_received_ = true;
    }
};

}  // namespace myapp

#endif
```

**步骤 2：注册到插件系统**

```cpp
// 在你的库中提供一个注册函数
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    registry->register_component<MyComponent>("myapp::MyComponent");
}
```

**步骤 3：编写 component.conf**

```yaml
# my_component.conf
class: myapp::MyComponent
library: libmyapp.so
module_dir: modules/myapp
executor_threads: 1
process_group: default
trigger:
  type: timer
  cycle_ms: 50
outputs:
  - name: trajectory
    topic: my.trajectory
    record: true
inputs:
  - name: pose
    topic: localization.pose
```

**步骤 4：在 module.conf 中引用**

```yaml
# module.conf
name: myapp
components:
  - class: myapp::MyComponent
    conf: modules/myapp/my_component.conf
```

**步骤 5：运行**

```bash
./star run --frames=100 myapp
./star monitor myapp
```

### 8.2 定义新的 Schema 类型

**步骤 1：编写 .schema 文件**

```schema
# documents/05_docs/spec/examples/my_data.schema
schema MyData {
    version = 1
    hash = 0xDEADBEEF
    root MyData

    def MyData {
        1: int64 timestamp
        2: double value
        3: string label
        4: Vec3D position
    }

    def Vec3D {
        1: double x
        2: double y
        3: double z
    }
}
```

**步骤 2：生成代码**

```bash
./star build  # codegen 自动运行
```

生成 `my_data.s.h`，包含 `MyData` / `Vec3D` 的 C++ 类型定义。

**步骤 3：在组件中使用**

```cpp
#include "documents/05_docs/spec/examples/my_data.s.h"

// 直接使用生成的类型
auto data = star::schema::MyData{};
data.timestamp = now_ns();
data.value = 3.14;
data.label = "test";
```

### 8.3 扩展 Transport 层

**添加新的 MessageBus 实现**：

1. 继承 `star::transport::MessageBus` 抽象接口
2. 实现所有纯虚函数（declare_topic / create_writer / create_reader / dump_stats_lines）
3. 在 `RuntimeFactory` 中注册新的 transport 名称
4. 通过 `./star run --transport=<your_name>` 使用

**示例**（UDP 传输）：

```cpp
class UdpMessageBus : public star::transport::MessageBus {
    // ... 实现 ...
};

// 在 factory 中注册
auto RuntimeFactory::create_udp_context(...) -> RuntimeContextPtr {
    auto bus = std::make_shared<UdpMessageBus>(...);
    return std::make_shared<RuntimeContext>(bus, ...);
}
```

### 8.4 代码规范摘要

详见 `.trae/rules/project_rules.md`，核心要点：

| 规则 | 要求 |
|------|------|
| 文件命名 | snake_case |
| 类名 | PascalCase |
| 变量/函数 | snake_case（查询/取值类） / CamelCase（操作/执行类） |
| 私有成员 | snake_case + 下划线结尾 |
| 头文件优先 | 所有代码在 .h 中 |
| 注释 | 默认不加，仅极端复杂处 |
| 硬编码禁止 | 数值/字符串/枚举值全部外部化 |
| Include 顺序 | 对应头 → 本模块 → 项目内 → 第三方 → 标准 |

---

## 9. 维护者参考

### 9.1 文件组织约定

```
basement/
├── <module>/              # 模块目录（snake_case）
│   ├── <feature>.h        # 功能头文件
│   └── *.h               # 子模块头文件（拆分出的）
apps/
├── types.h               # 共享类型（35 行以内）
├── main.h                # main() + parse_args（165 行以内）
├── app.h                 # App 类主体（500 行以内）
├── runner_utils.h        # 工具函数（100 行以内）
└── main.cc               # 单行 include
```

**大文件拆分判据**：只有当子模块**功能独立**时才拆，不为行数而拆。

### 9.2 构建系统内部

```
./star build
  → meson setup build/          # 配置（检测编译器、依赖）
  → meson compile -C build/     # 编译
  → run_command at configure:   # codegen（.schema → .s.h）
  → check_schema_generated.sh   # CI guard（产物校验）
```

**Build targets**: 10 个（basement 7 + apps 1 + tools 1 + codegen 1）

### 9.3 CI Guard

```bash
./star check              # 完整验证
./star check --skip-build # 跳过编译
./star check --verbose    # 详细输出
./star check planning     # 指定模块
```

三步验证：**Build → Doctor → List**

### 9.4 版本历史

| 版本 | 关键变更 |
|------|---------|
| v0.1.0 | 初始交付：Schema + Transport + Runtime 骨架 |
| v0.2.0 | F1-F6: Monitor / p99-p999 / RollingFileLogger / 日志格式 / IClock |
| v0.2.1 | TD-1+TD-2: 架构清理（删除 deprecated/ROS2/CMakeLists） |
| v0.2.2 | T1+TD5: SHM QoS 对齐 + main.cc 拆分 |
| v0.2.3 | O4+O1: Log 零开销 + native_message_bus.h 拆分 |
| v0.2.4 | O-3: shm_message_bus.h 拆分 |
| v0.2.5 | O-4: native_schema_loader.h 拆分 |
| v0.2.6 | O-5: native_component_host.h 拆分 + TD/F/O 命名规范 |
| v0.2.7 | **TD-4: CI Guard (`star check`)** |
| v0.2.8 | **F-1: ResourceMonitor 资源监控** |
| v0.2.9 | **F-2~F-4: HealthChecker + EventBus + ShmServiceBus** |

---

## A. 概念映射表

| Starletto | Apollo CyberRT | Autoware (ROS2) | ROS2 |
|-----------|---------------|-----------------|------|
| Component | Component | Node | Node |
| Channel (Topic) | Channel | Topic | Topic |
| ServiceBus | Service | — | Action/Service |
| EventBus | — | — | Parameter Event (部分) |
| Scheduler | Executor | Executor | Executor |
| Trigger | Timer/Routine | Timer | Rate/Timer |
| Manifest (conf) | Dag Config | Param | Launch/Param |
| Schema (.schema) | Proto (.proto) | — | Msg (.msg) |
| SchemaTypeId | MessageType | — | MessageType |
| NativeComponentHost | NodeBoard | LifecycleManager | LifecycleManager |
| ProcessOrchestrator | Mainboard | LaunchManager | Launch |
| RoutedMessageBus | RoutingMode | — | Remap |
| ResourceMonitor | — | — | — |
| HealthChecker | — | — | — |
| `star` CLI | cyber_launch | ros2 launch | ros2 launch |
| `./star build` | bazel build | colcon build | colcon build |

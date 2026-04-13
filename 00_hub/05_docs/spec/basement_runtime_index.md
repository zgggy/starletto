# Basement / Runtime 拆分索引（禁止大而全 runtime）

Runtime 的职责是"把模块跑起来并可控"：装配、调度、触发、进程编排、观测。
Runtime 必须拆分为多个子模块，避免形成一个不可维护的巨型目录。

## 1. Runtime 子模块拆分

- **runtime_context**
  - 目标：组件获取系统能力的唯一入口（transport/scheduler/parameter/service/logger/clock）
  - 约束：context 不得泄露后端类型（不得暴露 shm fd、线程实现等）
  - ✅ 已实现：`FrameContext` 集成（`current_frame()` / `set_current_frame()`），Scheduler 通过 `set_context()` 注入帧信息
- **component**
  - 目标：组件生命周期接口与注册机制、便捷数据访问 API
  - 约束：组件是普通 C++ 对象，不继承任何中间件对象
  - ✅ 已实现：
    - `Component` 基类：`declare_reader<T>(name, cb)` / `declare_writer<T>(name)` / `write(name, msg)`
    - **`auto_configure()`** — 从 conf 自动创建所有 reader/writer（零样板模式）
    - `read_latest<T>(name)` — 类型安全的最新消息读取（轮询模式）
    - `publish(name, msg)` — write 的别名，语义更清晰
    - `decode_snapshot_entry<Snap, T>(snap, key, schema)` — 通用 snapshot 条目解码模板方法
    - 自动 inproc/SHM 双路径解码（SFINAE 检测 from_binary）
    - `ComponentRegistry` + `schema_registry()` — 插件注册 + schema 自注册
    - `NativeComponentHost` — 装配器（manifest 解析 / trigger 绑定 / recorder 设置 / auto_configure 调用）
- **trigger**
  - 目标：消息同步触发 + 定时驱动；帧语义入口
  - ✅ 已实现：
    - `SyncTrigger` — 等 all required topic 到齐后触发 frame 回调
    - `TimerTrigger(callback, cycle_ms)` — 定时触发器，sleep_until 精确周期，支持 start/stop
  - ⬜ 后续：DAG barrier 集成、超时/丢帧策略
- **scheduler**
  - 目标：任务队列、优先级、线程池隔离、资源控制
  - ✅ 已实现：NativeScheduler（choreography + cpu_affinity + TaskStats + FrameContext 注入）
- **process**
  - 目标：多进程编排、监督、日志分离、健康检查、退出策略
  - ✅ 已实现：`ProcessOrchestrator`
    - `launch_worker(config)` — fork + execv 封装
    - `wait_all(pids, pid_group)` — waitpid + 结果收集
    - `build_route_plan(producers, consumers)` — SHM 路由计划构建
  - ⬜ 后续：health check / restart policy / 信号处理统一
- **observability**
  - 目标：统计采集（频率/延迟/丢帧）、trace、profiling
  - ✅ 已实现：
    - `ChannelStats` — 单 topic 统计（publish count/drop/payload size/interval）
    - `ObservabilityRegistry` — 全局 topic 统计注册表 + `dump_stats_lines()`
  - ⬜ 后续：trace/span 集成、profiling 接口

## 2. 依赖关系（从底到上）

`core` → `schema` → `transport` → `runtime_context` → `component` → `trigger/scheduler` → `host/process` → `tools`

**严格单向依赖，禁止反向引用。**

## 3. 当前代码映射

| Spec 子模块 | 实际文件 | 状态 |
|------------|---------|------|
| context | `basement/runtime/context/runtime_context.h` | ✅ |
| | `basement/runtime/context/frame_context.h` | ✅ |
| component | `basement/runtime/component/component.h` | ✅ (已增强) |
| | `basement/runtime/component/component_registry.h` | ✅ (+schema_registry) |
| | `basement/runtime/component/native_component_host.h` | ✅ (不再继承 ComponentHost) |
| trigger | `basement/runtime/trigger/trigger.h` | ✅ (+TimerTrigger) |
| scheduler | `basement/runtime/scheduler/scheduler.h` | ✅ |
| | `basement/runtime/scheduler/native_scheduler.h` | ✅ |
| process | `basement/runtime/process/process_orchestrator.h` | ✅ 新增 |
| observability | `basement/runtime/observability/channel_stats.h` | ✅ 新增 |
| factory | `basement/runtime/factory/runtime_factory.h` | ✅ |

### 已清理的过时代码

| 文件 | 处理 | 原因 |
|------|------|------|
| `basement/runtime/component/component_host.h` | 🗑️ 删除 | 冗余纯虚抽象，仅 NativeComponentHost 使用 |
| `basement/core/definition.h` | 🗑️ 删除 | 全局常量迁移到 `libraries/element/math_constants.h` |
| `basement/README.md` | ✏️ 重写 | 从 ROS2 架构文档更新为 native-first 手册 |

## 4. 用户组件编写指南

### 模式 A：零样板（推荐，auto_configure 驱动）

```cpp
class MyComponent : public star::runtime::Component {
  public:
    // init() 可为空！auto_configure 从 conf 自动创建 reader/writer
    bool process_once() override {
        auto input = read_latest<InputMsg>("input_name");
        auto output = processor_.compute(input ? *input : InputMsg{});
        publish("output", output);
        return true;
    }
};
```

### 模式 B：回调式（需要自定义处理逻辑时）

```cpp
class MyComponent : public star::runtime::Component {
  public:
    void init() override {
        declare_reader<InputMsg>("input", [this](auto msg) { on_input(msg); });
        // writer 由 auto_configure 自动创建
    }
};
```

对比旧版（~250 行样板代码）→ P0 增强（~30 行）→ **auto_configure（~15 行有效代码）**。

## 5. 下一步推进顺序

1) ✅ Schema/Transport 契约固化
2) ✅ trigger/scheduler 对齐（choreography + SyncTrigger + FrameContext）
3) ✅ M4 planning→control 最小闭环
4) ✅ M5 Record/Replay STARREC 格式
5) ✅ M6 CLI 工具 + 多进程 SHM 隔离验证
6) ✅ P0 Component 基类增强（declare_reader/write/decode_snapshot_entry）
7) ✅ Process 编排模块 + Observability + TimerTrigger
8) ✅ **P7 auto_configure 零样板模式**（conf 驱动 reader/writer 自动创建 + read_latest/publish）
9) ⬜ process: health check / restart / 日志策略
9) ⬜ observability: trace/span 集成
10) ⬜ old/ 迁移（25 个文件从 basement/old/ 迁移到新架构）

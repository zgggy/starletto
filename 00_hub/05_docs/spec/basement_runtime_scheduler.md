# Basement / Runtime / Scheduler 设计方案（对齐 Apollo classic/choreography）

Scheduler 的职责：把触发产生的任务按策略调度到执行资源上，提供优先级、资源隔离、观测与后续的帧一致性/DAG 能力。

## 1. 目标能力（对齐 Apollo CyberRT）

Apollo scheduler 提供 classic 与 choreography 两类策略（参见 Apollo 文档）。  
Starletto 要对齐的不是“配置语法”，而是“可表达的能力边界”：

- 多 group（资源隔离、跨 NUMA 隔离）
- 每组独立线程池/亲和（range/1to1）
- 任务优先级队列（避免优先级反转）
- 线程调度策略（SCHED_FIFO/RR/OTHER）与优先级
- choreography：主链路任务编排 + pool 兜底

参考：`/home/zgy/projects/apollo/docs/04_CyberRT/CyberRT_Scheduler_cn.md`

## 2. Starletto 的 Scheduler 分层

- **Scheduler API**：运行时接口（register_task/notify/start/stop/configure）
- **Policy**：classic/choreography 等策略实现
- **Executor**：线程池/协程池（执行载体）
- **Queue**：多优先级队列、work-stealing（可选）
- **Stats**：任务耗时、排队时间、触发到执行延迟

## 3. 当前实现与差距

现状：

- 已实现最小 `NativeScheduler`：周期任务 + notify 触发 + priority queue
- 已支持 `process_group` 的 scheduler 隔离（每组一个 scheduler）
- ✅ 已实现 `cpu_affinity`：通过 `pthread_setaffinity_np` 应用于 worker 线程（格式 "0-3" / "0,1,2,3"）
- ✅ 已实现 `TaskStats`：per-task 执行统计（invocations / avg_wall_time_us / last_wall_time_us）
- ✅ 已实现周期性 stats dump（start 时 + 每 60 秒自动输出）

剩余差距（P2 及以后）：

- ⬜ 缺 classic/choreography 的可配置表达（当前 choreography 为默认有效策略）
- ✅ 帧语义 FrameContext 已实现（M3 完成，见 §5）
- ⬜ DAG barrier（M4 闭环时实现）
- ⬜ 缺 SCHED_FIFO/RR 调度策略（当前使用 SCHED_OTHER）

## 4. 配置模型（schema typed plan）

必须在 component/module manifest 中表达：

- `process_group`
- `executor_threads`
- `scheduler_type`（classic/choreography）
- `priority`
- `cpu_affinity`（string，格式如 "0-3" 或 "0,1,2,3"，应用于 worker 线程）
- `task_name`（显式任务命名）

禁止在 runtime 中引入"另一个配置体系"。

### 4.1 typed plan 完全贯穿（✅ 已完成）

本轮 typed plan 深化已实现：

1) ✅ `NativeScheduler::apply_plan()` **处理** `cpu_affinity`：
   - 格式：CPU list 或 range（如 `"0-3"`, `"0,1,2,3"`）
   - 生效对象：worker 线程（通过 `pthread_setaffinity_np`）
   - 解析失败时记录警告，不阻塞运行
   - `cpu_affinity` 字段位于 `ChoreographyPlan`（非 TaskSchedulePlan），对组内所有 worker 统一生效

2) ✅ `scheduler_type` 语义明确：
   - `choreography`（默认）：当前实现有效，pool 线程池并发执行
   - `classic`：保留接口，后续 P2 实现；本轮仅记录 `scheduler_type_`，不改变调度行为

3) ✅ **ComponentManifest 过渡桥已完全移除**：
   - `NativeComponentHost::load()` 不再调用 `set_manifest()`
   - `Component::manifest_` / `set_manifest()` / `manifest()` 已删除
   - `ComponentHost` 接口从 `load(ComponentManifest)` 改为 `load(ComponentLoadPlan)`
   - 所有组件只依赖 `ComponentLoadPlan` 的 typed 字段（见 basement_runtime_host.md §4.1）

### 4.2 TaskStats 合约（✅ 已实现）

每个注册到 scheduler 的任务自动维护执行统计：

```cpp
struct TaskStats {
    std::string task_name;
    uint64_t total_invocations{0};
    uint64_t total_wall_time_us{0};
    auto avg_wall_time_us() const -> double;  // total / invocations
    uint64_t last_wall_time_us{0};
};
```

输出时机：
- `start()` 时输出初始状态（所有任务 invocations=0）
- timer 线程每 60 秒自动 dump 一次
- 可通过 `dump_stats()` 手动触发

deadlock-safe 设计：
- `dump_stats_unlocked()` 变体供已持锁调用方使用（如 `start()` 内部）

## 5. 帧一致性（FrameContext）与 DAG（路线）

目标：对齐 Apollo 的“帧级一致性调度”思想：

- 以主消息时间戳或 frame_id 作为帧标识
- 触发器产生 frame_id，调度器按帧推进
- DAG barrier：同帧前置任务完成后才放行后置任务

## 5. FrameContext 与帧语义（M3 ✅ 已实现）

### 5.1 FrameContext 定义（✅）

```cpp
struct FrameContext {
    int64_t frame_id{0};            // 单调递增帧号
    int64_t monotonic_time_ns{0};   // CLOCK_MONOTONIC（帧启动时刻）
};
```

**设计约束：**
- `frame_id` 从 1 开始单调递增（与现有 `TaskState.next_frame_id` 对齐）
- `monotonic_time_ns` 在 Scheduler 推送任务到 ready queue 时采样
- 同一帧内所有任务的 `frame_id` 相同，`monotonic_time_ns` 相同

### 5.2 RuntimeContext 集成（✅）

`RuntimeContext` 新增接口：

```cpp
class RuntimeContext {
public:
    // ... 现有接口 ...

    auto current_frame() const -> const FrameContext&;
};
```

- 返回当前线程正在处理的 FrameContext 引用
- 非 worker 线程调用时返回默认值 `{frame_id:0, monotonic_time_ns:0}`

### 5.3 Scheduler 注入机制（✅）

Scheduler 在调用 worker callback **之前** 绑定 FrameContext：

```
timer_loop / notify → push_ready(task_name, frame_id)
    → 记录 monotonic_time_ns → 构建 FrameContext
    → worker_loop 取出 ReadyItem
        → 绑定 FrameContext 到 RuntimeContext
        → 调用 descriptor.callback(frame_id)
        → 解除绑定
```

### 5.4 多进程约束（✅）

- 每个 process group 的 Scheduler 维护独立的 frame_id 序列
- Timer trigger 各自驱动自己的帧推进（不跨进程同步）
- M4 闭环时再引入跨进程帧同步（由 sensor input 或外部 clock 驱动）

### 5.5 验收命令（✅ 已通过）

```bash
./star build
# single-process: MotionComponent 日志应包含 frame_id 信息 ✅
./star run planning | grep "MotionComponent processing frame="
# 输出: [I] MotionComponent processing frame=1 / frame=2 / ...
# multi-process: EXIT=0 无崩溃（子进程日志隔离）
./star run --multi-process --frames=10  # EXIT=0
```

## 6. 验收标准

P1（✅ 已完成）：

- ✅ priority 与 group 隔离有效
- ✅ scheduler/trigger 可输出任务统计（TaskStats：执行次数、耗时均值、最近耗时）
- ✅ `cpu_affinity` 字段从 plan 解析并在 worker 线程生效（`ChoreographyPlan.cpu_affinity` → `pthread_setaffinity_np`）
- ✅ typed plan 完全贯穿：组件不依赖 ComponentManifest（见 basement_runtime_host.md §4.1）
- ✅ FrameContext 帧语义：Scheduler 在 callback 前注入 `{frame_id, monotonic_time_ns}`，Component 通过 `context()->current_frame()` 读取
- ✅ 验收命令：
  - `./star build`
  - `./star run planning` → 日志包含 `MotionComponent processing frame=N`
  - `./star run --multi-process --frames=10` → EXIT=0 无崩溃
  - 日志应包含 `[SchedulerStats] task=..., invocations=..., avg_time_us=...`

P2：

- classic/choreography 策略可表达并可运行
- 帧一致性 + DAG barrier 跑通 planning 主链路样板

### 6.1 Scheduler 任务统计最小接口（typed plan 深化 P1.1）

`NativeScheduler` 必须通过日志输出以下统计信息（在 `start()` 时和周期性地）：

```cpp
// 统计结构（内嵌于 NativeScheduler，不对外暴露）
struct TaskStats {
    std::string task_name;
    uint64_t total_invocations;     // 总执行次数
    double total_wall_time_us;       // 累计墙上时间（微秒）
    double last_wall_time_us;        // 最近一次墙上时间（微秒）
    uint64_t total_queue_depth;      // 累计入队次数（≈ total_invocations）
    double avg_wall_time_us() const; // = total_wall_time_us / total_invocations
};

// 统计输出格式（logI）
"[SchedulerStats] task=<name>, invocations=<N>, avg_time_us=<T>, last_time_us=<L>"
```

日志输出时机：
- `start()` 时：输出所有注册任务的初始统计（invocations=0）
- 每 60 秒：周期性输出所有任务统计

### 6.2 本轮最小验收命令

```bash
# 必须通过
./star build
./star schema lint --schema=modules/setting/schema/planning/motion.schema
./star doctor planning
./star run --frames=2 planning

# cpu_affinity 验证（如果有配置）
STAR_SCHEDULER_AFFINITY=0-3 ./star run --multi-process --frames=6 planning
# 确认 worker 线程绑定到指定 CPU
```

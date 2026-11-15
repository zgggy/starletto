# basement/runtime —— 组件运行时

把组件装配起来并可控地跑：生命周期、端口、plan、调度、触发、进程编排、观测。

## 目录

| 路径 | 作用 |
|---|---|
| `component/` | `Component` 基类、`ComponentRegistry`、插件加载、`NativeComponentHost` |
| `context/` | `RuntimeContext`/`FrameContext`，组件访问系统能力的唯一入口 |
| `plan/` | `ComponentLoadPlan`、`ProcessPlan`、严格 conf 校验、拓扑类型检查 |
| `scheduler/` | 单线程/多线程执行器、任务统计 |
| `trigger/` | timer/channel/sync_topic 触发 |
| `process/` | 多进程 supervisor、ready 握手、重启策略、路径解析 |
| `observability/` | 健康检查、stats/health 发布、资源采样（Linux/macOS） |
| `parameter/` | 进程内参数存储 |

## 组件装配（唯一路径）

1. `component.conf` 声明 readers/writers/commands/trigger；
2. host 构造 `ComponentLoadPlan`，设置 context/plan；
3. `auto_configure()` 建好 reader/writer/service；
4. `init()` 只做非端口初始化；
5. 调度器调用 `process_once(inputs)`：trigger 消息随 `FrameInputs` 传入，
   可用 `TypedComponent<Derived, TriggerInput<T, "port">...>` 直接收 typed 参数；非 trigger 用 `read_latest<T>()` / `write()`。

没有 `declare_reader/writer`，也没有第二套端口创建路径。

## 动态与静态

- 动态：`native_runner` 按 `component_library` dlopen 插件；
- 静态：`native_runner_static` 链接 `star_static_register_*` 聚合库，
  `static_link=true` 时绝不 dlopen；
- 两种方式共用同一份 conf/plan/组件代码。

测试：`runtime_component`、`runtime_plugin_abi`、`runtime_plan`、`runtime_scheduler`、
`runtime_trigger`、`runtime_process_orchestrator`、`runtime_health_monitor`、
`runtime_health_checker`、`runtime_stats_publisher`、`runtime_resource_monitor`、
`runtime_parameter_store`、`runtime_frame_robustness`、`runtime_wire_identity`。

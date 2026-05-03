# Starletto 开发计划

> **工程边界**：Starletto 由三个独立工程组成：
>
> 1. **basement/** — 运行时底座（本计划跟踪范围）
> 2. **libraries/** — 通用库（math/io/blackboard 等，独立 spec）
> 3. **modules/** — 业务模块（planning/control/setting 等，独立 spec）
>
> **当前聚焦**：basement。libraries 和 modules 的改动仅用于配合 basement 开发与测试。
>
> **Basement 进度总览**：详见 [basement_progress.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_progress.md)

## 0. Basement 当前状态（2026-04-07）

**整体成熟度：~85%**

| 子模块 | 成熟度 | 状态 |
|--------|--------|------|
| core | 80% | 基础完备，缺 RollingFileLogger / 时间抽象 |
| schema | **100%** | ✅ IDL/codec/codegen/CI 全闭环 |
| transport | 90% | 核心完整，缺 async reader / SHM QoS / p99 |
| runtime | 85% | auto_configure 强大，缺 supervisor / DAG / health |
| manifest | 60% | 最大技术债务：ConfValue 中间载体未消除 |
| tools | 85% | record/replay/schema CLI 就绪，缺 monitor |

**下一步最大价值点：B1 Manifest Schema 化**（消除 ConfValue 依赖）

## 1. 里程碑总览

- **M0（已完成雏形）**：Native-only 主路径 + manifest 装配 + install-tree 运行 + inproc/shm 零拷贝骨架 + 基础工具链
- **M1（Schema v1 ✅ 完成）**：`.schema` IDL + codegen/codec/fmt/lint/compat 全链路；`star schema *` CLI 工具；两条 snapshot 全走 binary 路径；跨进程 shm zero-copy 已验证
- **M2（Transport v1 ✅ 完成）**：完全以 schema 体系驱动 channel/service/record；inproc 指针 + shm loan/view；futex 事件通知；shm 容量硬编码常量；观测统计已实现；**routed 去 env 注入**（runner→worker 参数传递）
- **M3（Runtime v1 ✅ 完成）**：choreography 可表达；cpu_affinity 生效；TaskStats 统计输出；typed plan 单轨制（manifest bridge 已移除）；**FrameContext 帧语义注入链路**；**SyncTrigger 多 topic 同步触发器**
- **M4（系统闭环 ⬜ 未开始）**：sensor→planning→control 最小闭环 + record/replay 可复现

## 2. M1：Schema v1（第一优先级）

Spec： [basement_schema.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_schema.md)

交付项：

- IDL v1：引入字段 id、enum/oneof/map/repeated/optional/default、reserved
- Codec v1：二进制 wire format（tag/len/payload），unknown field 保留
- Codegen v1：C++ `View/Builder/Descriptor/Codec glue`
- 工具链 v1：fmt/gen/compat/lint（至少兼容性检查可用）

验收：

- ✅ 两条系统级 snapshot（parameter/config）不再走 ConfValue 通路（binary_only > 0）
- ✅ 跨进程通过 shm loan/view 完成 zero-copy（业务侧不再 dump/parse 文本）
- ✅ IDL 解析器（schema_codegen.py 1052 行，支持完整 IDL 语法，Meson 自动集成生成 .s.h）
- ✅ Compat 工具（schema_compat.py 兼容性检查 + `star schema compat` CLI）
- ✅ 工具链 CLI（`star schema gen/fmt/lint/compat` 四个子命令全部可用）

## 3. M2：Transport v1（第二优先级）

Spec： [basement_transport.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)

交付项：

- Channel/Reader/Writer 术语与接口统一输出（工具与日志）
- inproc：TypedConstPtr 指针传递（禁止序列化）
- shm：loan/commit + view（对齐 schema builder/view）
- routed：同组 inproc、跨组 shm 自动路由（不依赖 env）
- 观测：频率、payload size、延迟、丢帧统计对外输出（monitor）

验收：

- 多进程 planning 样板链路稳定运行，并输出关键 channel 指标

## 4. M3：Runtime v1（第三优先级，拆分推进）

Spec 索引： [basement_runtime_index.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_runtime_index.md)

交付项（按拆分子模块推进）：

- Host：装配日志完善、错误诊断完善、install-tree 行为一致
- Scheduler：✅ choreography 可表达，任务统计输出（TaskStats），cpu_affinity 生效
- Trigger：✅ **SyncTrigger 多 topic 同步触发器**（`sync_topic` 类型 / 多 reader 自动检测）
- Context：✅ **FrameContext 帧语义**（`{frame_id, monotonic_time_ns}` 注入链路）
- Process：⬜ 监督/重启策略、health check、日志策略固化

验收：

- ✅ 多进程下 process_group 的资源隔离可验证（线程数/优先级/日志）
- ✅ 一个"帧一致性"样板链路跑通（setting→planning，按 frame_id 推进，日志输出 `MotionComponent processing frame=N`）

## 5. M4：完整系统闭环（✅ 全部完成）

目标：

- ✅ **planning → control 最小闭环已跑通**（MotionProcessor→TrajectoryOutput→PidController→ControlCommand）
- ✅ **SyncTrigger 消息驱动触发**（trajectory 发布 → control task 自动调度）
- ✅ **端到端延迟测量**（84~179µs 同进程 NativeMessageBus，LatencyStats 每20帧dump）
- ✅ **DAG 顺序保证**（SyncTrigger + 优先级队列隐式保证 planning→control 执行顺序）
- ✅ **Record/Replay 基础能力**（MessageRecorder STARREC 二进制格式 + NativeMessageBus unique_ptr 拥有 + conf `record=true` 字段驱动）
- localization/perception/prediction 的 stub 接入
- 关键指标：端到端延迟、丢帧、CPU/内存、稳定性

拆分建议（模块侧）：

- setting：parameter/config/health ✅
- planning：motion ✅（输出 TrajectoryOutput，含 20 点 2 秒规划时域）
- control：✅ PID 控制器已实现（订阅 trajectory → 计算 throttle/brake/steering/gear → 发布 ControlCommand + 延迟统计）
- playback：✅ **record 基础能力已完成**（MessageRecorder + MessageReplayer + Bus集成）

验收：

- ✅ 单进程四组件闭环运行无崩溃（config/parameter/motion/control）
- ✅ 多进程跨进程 trajectory→control 验证（SyncTrigger 触发，完整数据流跑通）
- ✅ 端到端延迟测量（e2e_latency_us: 84~179µs, LatencyStats: count/avg/min/max 每20帧dump）
- ✅ **消息录制验证通过**（`record=true` → STARREC 二进制文件 244bytes/2条消息，含 topic/type_name/timestamp/payload）

## 5.5 M6：CLI 工具 + 多进程隔离（✅ 全部完成）

目标：

- ✅ **CLI record/replay 基础能力**（MessageRecorder + MessageReplayer + STARREC 格式）
- ✅ **多进程 SHM 路由验证**（planning/control 进程组通过 shm_routed 通信，e2e_latency_us: 300~700μs）
- ✅ **ProcessOrchestrator**（fork/execv/waitpid/route-plan 封装，独立头文件）
- ✅ **path_resolver 抽取**（8 个路径解析函数从 main.cc 移至 `basement/runtime/process/path_resolver.h`）

验收：

- ✅ `./star run --multi-process planning... control...` 双进程闭环运行
- ✅ SHM cross-group topic 自动检测与路由
- ✅ native_runner/main.cc 瘦身 ~300+ 行（path_resolver + process_orchestrator + schema_bootstrap）

## 5.6 P0-P6：Runtime 增强与系统清理（✅ 全部完成）

- ✅ **P0 Component 基类增强**：`declare_reader<T>` / `declare_writer<T>` / `write()` / `decode_snapshot_entry<Snap,T>` — 用户代码减少 50%+
- ✅ **P1 basement/README.md 重写**：native-first 架构手册
- ✅ **P2 删除 component_host.h**：合并到 native_component_host.h
- ✅ **P3 definition.h 迁移**：`basement/core/definition.h` → `libraries/element/math_constants.h`
- ✅ **P4 ProcessOrchestrator 抽取**：进程管理逻辑独立模块
- ✅ **P5 Observability（ChannelStats）**：per-topic 发布频率/payload/间隔统计
- ✅ **P6 TimerTrigger**：sleep_until 精确周期触发器
- ✅ **schema_bootstrap.h**：16 业务 schema 原子去重注册，runner 单点调用
- ✅ **Spec 同步**：basement_runtime_index.md 全面更新

## 6. ConfValue 淘汰计划（硬约束）

- ✅ M1 后：禁止 ConfValue 出现在 channel/service 的业务载体上（snapshot 已全走 binary）
- 🟡 M2 后：manifest 迁移为 schema typed（清单即类型）— **manifest bridge 已从代码中移除**，但 `component_manifest_from_load_plan()` 等反向转换函数仍保留供内部使用
- M3 后：ConfValue 仅保留为 debug_text codec 与"导入工具"
- 当前残留 ConfValue 使用点需审计（`./star doctor --deep` 可辅助发现）

## 7. P7-P5：Transport 增强与零样板（✅ 全部完成）

### 7.1 P7 auto_configure 零样板模式（2026-04-06）

Spec: [basement_runtime_host.md §7](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_runtime_host.md)

交付项：
- ✅ `Component::auto_configure()` — 从 `component.conf` 的 `load_plan` 自动创建所有 reader/writer/delivery_writer/bidirectional_channel/service
- ✅ `read_latest<T>(name)` — 类型安全轮询 API，inproc 指针路径优先 + SHM from_binary 回退
- ✅ `publish(name, msg)` / `write(name, msg)` — writer 别名
- ✅ motion_component.h 重写为 Mode A（零样板，~103 行）
- ✅ control_component.h 重写为 Mode B（callback + auto_configure writer）

验收：
- ✅ `./star run --frames=3 modules/setting/config/component.conf` 正常运行

### 7.2 P1 DeliveryWriter（2026-04-06）

Spec: [basement_transport.md §10](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)

交付项：
- ✅ `DeliveryResult { accepted, message, latency_us }`
- ✅ `DeliveryWriter : Writer` 抽象接口 + `write_with_delivery(msg, timeout) → future<DeliveryResult>`
- ✅ `InprocSyncWriter` 同步 dispatch-all-readers 实现
- ✅ Component 层 `declare_delivery_writer<T>()` / `write_with_delivery<T>()`

验收：
- ✅ 编译通过 + 单进程/多进程测试正常

### 7.3 P2 Bidirectional Channel（2026-04-06）

Spec: [basement_transport.md §11](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)

交付项：
- ✅ `EndpointPlan.direction = bidirectional` schema 字段
- ✅ `auto_configure` 双向通道自动创建 Reader+Writer
- ✅ `read_channel<T>()` / `write_channel<T>()` API

验收：
- ✅ 编译通过 + 测试正常

### 7.4 P3 Service conf 集成（2026-04-06）

Spec: [basement_transport.md §13](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)

交付项：
- ✅ `ServiceEndpointPlan` schema 定义（name/topic/request_type_name/response_type_name/role）
- ✅ `ComponentLoadPlan.services` 字段
- ✅ `auto_configure` service 处理（server→ServiceServer+handler dispatch, client→ServiceClient）
- ✅ `call_service<Req,Res>(name, request) → Res` 类型安全模板方法
- ✅ parameter_service_demo 全链路验证

验收：
- ✅ `./star param demo` list/get/set 全链路正常

### 7.5 Blackboard 迁移（2026-04-06）

交付项：
- ✅ `libraries/common/blackboard/blackboard.h` — 从已删除的 `basement/old/io/` 迁移
- ✅ 20 个文件引用更新（namespace: `star::common::Blackboard`）
- ✅ 依赖方向验证：basement → libraries 禁止 ✅

### 7.6 old/ 目录清理（2026-04-06）

交付项：
- ✅ 删除 `basement/old/` 整个目录（-26 文件, -6323 行）
- ✅ 4 个遗留 ROS2 Component 文件标记 @deprecated（motion.h/refline.h/parameter.h/debug_motion.h）
- ✅ `basement/CMakeLists.txt` 标记 @LEGACY（Meson 为唯一构建系统）

## 8. Codegen 构建集成（✅ 完成）

Spec: [basement_schema.md §12](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_schema.md)

交付项：
- ✅ Meson `run_command()` 在 configure 时自动执行 codegen（20 对 .schema → .s.h）
- ✅ 批量目录模式 `--schema-dir` / `--output-dir`（新增功能，替代手动维护的 schema_pairs 列表）
- ✅ 新增 .schema 文件无需修改 meson.build（自动发现）
- ✅ CI 守护：`tools/check_schema_generated.sh` + `.git-hooks/pre-commit` hook
- ✅ spec §12.5 全部 4/4 验收项勾选

验收：
- ✅ `rm -rf build && ./star build` → 15/15 targets, zero errors
- ✅ 干净状态 CI 脚本 pass；手动篡改 .s.h 被 block commit

## 9. P4-P5：生产级能力增强（✅ 完成）

### 9.1 P4 Correlation ID / 异步调用（2026-04-07）

Spec: [basement_transport.md §14](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)

交付项：
- ✅ `CorrelationId` 结构体 + `operator==` + `std::hash` 特化 + `generate()` 原子生成器
- ✅ `ServiceRequest` / `ServiceResponse` 新增 `.correlation_id` 字段
- ✅ `ServiceClient::async_call()` 纯虚接口 + `NativeClient` 实现
- ✅ `NativeServiceBus` pending_calls_ map + async_call_service/fulfill_pending/reject_pending
- ✅ `Component::async_call_service<Req, Res>(name, req) → std::future<_Res>` 模板方法
- ✅ 向后兼容：现有 `call()` / `call_service()` 行为不变

验收：
- ✅ Clean build 15/15 + 单进程 config + 多进程 SHM planning + parameter_service_demo 全通过

### 9.2 P5 QoS / 流控（2026-04-07）

Spec: [basement_transport.md §15](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)

交付项：
- ✅ `DropPolicy` 枚举（KEEP_LAST / DROP_NEWEST）+ `QoSPolicy` 结构体
- ✅ `TopicDescriptor.qos` 字段 + `EndpointPlan` schema 扩展（qos_depth/qos_drop_policy）
- ✅ `NativeMessageBus` per-reader ring buffer + drop 策略实现
- ✅ `TopicStats` 新增 dropped_count / max_queue_depth
- ✅ `Component::auto_configure` 从 conf 读取 QoS 并传递给 TopicDescriptor
- ✅ `Component::read_history<T>(name, max_count)` 模板方法

验收：
- ✅ Clean build 15/15 + 全部端到端测试通过

## 10. 当前状态总览（2026-04-07）

| 能力域 | 状态 | 关键指标 |
|--------|------|----------|
| Schema IDL + Codegen | ✅ | 22 schema, 自动发现, CI 守护 |
| Transport (Channel/Reader/Writer) | ✅ | inproc 指针, SHM loan/view, QoS 流控 |
| Transport (Service) | ✅ | 同步+异步, Correlation ID, conf 驱动 |
| Runtime (Host/Scheduler/Trigger) | ✅ | choreography, SyncTrigger, FrameContext |
| Runtime (auto_configure) | ✅ | 零样板, reader/writer/service 自动创建 |
| Manifest (装配) | ✅ | install-tree, process_group |
| Tools (record/replay/monitor) | ✅ | STARREC 格式, stats dump |
| 代码卫生 | ✅ | old/ 删, deprecated 标记, CMake legacy |

**Basement 下一阶段（详见 [basement_progress.md §5](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_progress.md)）：**

| 优先级 | 阶段 | 目标 | 投入产出比 |
|--------|------|------|-----------|
| **P0** | **B1 Manifest Schema 化** | 消除 ConfValue 中间载体，manifest 全走 schema typed | 🔴 高（消除最大技术债务） |
| P1 | **B2 Core 增强** | RollingFileLogger + 统一日志格式 + Clock 抽象 + ByteSpan | 🟡 中（生产级基础设施） |
| P2 | **B3 Transport 观测增强** | Latency p99/p999 + per-component monitor + star monitor CLI | 🟡 中（可观测性） |
| P3 | **B4 Runtime 进程治理** | Supervisor + 健康检查 + DAG 调度雏形 | 🟢 低（可靠性） |
| P4 | **B5 测试基础设施** | catch2/googletest + 单元测试 + CI pipeline | 🟢 低（回归保障） |

> 注：M4 深化（localization/perception/prediction stub 接入）属于 **modules 工程**，不在本计划范围内。

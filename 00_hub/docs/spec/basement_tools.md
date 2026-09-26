# Basement / Tools 设计方案（star + runner + schema 工具链）

Tools 的目标是形成“工程可交付闭环”：能编译、能安装、能运行、能诊断、能记录回放、能观测。

## 1. 工具分层

- **star**：用户入口（build/install/run/list/graph/doctor/record/replay/monitor）
- **native_runner**：运行入口（装配并运行组件；支持 worker 形态）
- **schema 工具链**：fmt/gen/compat/lint

## 2. 现状

已具备：

- `star build/run/install`
- `star list/graph/doctor`（含 `doctor --deep`）
- `native_runner` 支持 `--list/--graph/--doctor`
- `star param`：typed parameter service 工具入口（list/get/set）

## 3. Schema 工具链（必须优先补齐）

对齐 protobuf 的最低工具集：

- `star schema fmt`：格式化 IDL（稳定输出）
- `star schema gen`：生成 C++（View/Builder/Descriptor/Codec glue）
- `star schema compat old new`：兼容性检查（breaking changes）
- `star schema lint`：风格与约束检查（id 是否重复、reserved 等）

## 4. Graph/Doctor（对齐 cyber_channel/cyber_monitor）

### 4.1 graph

必须支持：

- module 依赖图（已具备 dot 输出）
- component/topic 拓扑图（已具备 dot 输出）
- 后续扩展：标注 TypeId、process_group、qos、record 标记

### 4.2 doctor

必须支持：

- schema/manifest 校验（字段完整性、trigger 合法）
- plugin 可加载性（dlopen/dlsym）
- 运行环境检查（install layout、env、权限）
- 后续扩展：shm 通道检查与统计输出

## 5. Record/Replay（✅ 基础能力已实现，CLI 集成中）

record/replay 是自动驾驶系统工程化的必备能力：

- ✅ **record**：把指定 topic 的 wire bytes 记录到文件（带 timestamp/type_name/topic）— `basement/transport/record/message_recorder.h`
  - STARREC 二进制格式（magic + version + message_count + entries）
  - 每条 entry: timestamp_ns(8) + topic_name(4+len) + type_name(4+len) + payload(4+len)
  - NativeMessageBus 以 unique_ptr 拥有 Recorder，conf 的 `record=true` 字段驱动
- ✅ **replay**：按时间戳回放，支持速率控制、单 topic 过滤 — `basement/transport/record/message_replayer.h`
  - `play_all()` / `play_topic()` / 可配置 speed 参数
- ✅ **多进程 SHM 路由**：`--multi-process` + `--route-plan` 参数传递，无需 env 注入
- ⬜ 集成到 runner（`star record` / `star replay` 子命令）

关键要求：

- ✅ 记录格式以 schema wire format 为核心（不是 ConfValue）
- ✅ 多进程录制验证通过（planning→control 双进程，STARREC 文件正常写入）
- ⬜ 录放必须可复现线上行为（同版本、同参数、同输入）— 基础设施就绪，待端到端集成

### 5.1 当前 CLI 用法

```bash
# 单进程运行 + 自动录制（conf 中 record=true 的 topic）
./star run --frames=100 motion/component.conf control/component.conf

# 多进程运行 + SHM 路由
./star run --frames=100 --multi-process planning/motion/component.conf control/pid/component.conf

# 手动启动 worker（调试用）
./build/apps/native_runner/native_runner \
  --worker --transport=shm_routed \
  --process-group=planning \
  --route-plan="planning.trajectory|planning|control" \
  --frames=100 \
  modules/planning/motion/component.conf
```

## 6. Monitor/Trace（对齐 cyber_monitor + profiler）

必须具备：

- topic 频率、payload size、延迟统计
- scheduler/trigger 的执行统计（任务耗时、排队时间）
- process/线程资源统计（cpu、内存、affinity）

## 7. 验收标准

P1：

- ✅ `star list/graph/doctor` 输出信息包含 TypeId 与 process_group
- ✅ `star run --multi-process` + logs 分组可用（双进程 SHM 路由验证通过）
- ✅ `star param list/get/set` 走 typed service request/response 主路径

P2：

- ⬜ `star record/replay` 基于 schema wire format 可用（基础设施就绪，待 CLI 子命令封装）
- ✅ `star schema fmt/gen/compat` 形成闭环（像 protoc + buf 一样可用）

P3（Runtime 增强相关）：

- ✅ Component 基类 API：`declare_reader` / `declare_writer` / `write` / `decode_snapshot_entry`
- ✅ native_runner 瘦身：path_resolver + process_orchestrator + schema_bootstrap 独立模块
- ✅ Observability：ChannelStats per-topic 统计
- ✅ TimerTrigger + SyncTrigger 双触发模式

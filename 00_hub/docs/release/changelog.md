# 变更日志

## [v0.2.3] - 2026-04-11

### 收尾深化（O4 + O1 + TD3 重评估）

- **O4**: 日志宏零开销优化 — `for-loop` 短路模式
  - 新增 `Logger::is_enabled(level)` 静态方法（编译期短路）
  - `logD` 在 release 模式（level >= INFO）下：**零函数调用、零对象构造、零格式化**
  - 实现方式：`for (bool ok = is_enabled(level); ok; ) LogStream(...)` — 编译器完全消除 false 分支

- **O1**: native_message_bus.h 拆分 — 471 行 → 3 文件
  - `native_message_bus.h` (**250 行**) — 主类 + TopicStats/TopicState + compute_percentile + publish_internal + dump_stats
  - `native_writer.h` (**60 行**) — NativeWriter + InprocAckWriter
  - `native_reader.h` (**170 行**) — NativeReader
  - 工厂方法 (`create_writer/create_ack_writer/create_reader`) 声明在类内、实现在文件末尾（解决嵌套类前向引用问题）
  - friend 声明授予 Writer/Reader 访问私有方法权限

- **TD3 重评估**: schema_value.h 不是可清理债务
  - 被 **35 个文件**广泛引用（basement + libraries + modules + apps）
  - 是活跃的核心类型系统，不是 ConfValue 兼容层残留
  - 状态：从"待清理"改为"正式资产，Libraries 阶段再考虑重构命名

### 验证

- `./star build` ✅ 10/10 targets, 0 errors

---

## [v0.2.2] - 2026-04-11

### Transport 收尾 + main.cc 拆分（收尾优先）

- **T1**: SHM QoS buffer 扩展 — ShmMessageBus 新增 per-reader QoS 能力
  - `reader_history_` map (与 NativeMessageBus 对齐)
  - DropPolicy (KEEP_LAST / DROP_NEWEST) 在 deliver_shm_view 中生效
  - `latency_samples` 采样 + p99/p999 输出
  - `compute_percentile()` / `reader_history_size()` 公共 API
  - **效果**：Transport SHM 路径达到与 inproc 同等级别 → **Transport 100%**
- **T2 确认**: Async Reader — SHM 路径已有 poll_loop + futex 异步；inproc 路径同步 by design（零拷贝约束）
- **TD5**: main.cc 拆分 — 860 行单文件 → 4 个文件 (794 行)
  - `types.h` (35 行) — RunOptions / Inputs / g_argv0 声明
  - `main.h` (165 行) — main() 入口 + parse_args + crash_handler
  - `app.h` (508 行) — App 类全部 subcommand 实现
  - `runner_utils.h` (85 行) — manifest/path/library 解析工具函数

### 验证

- `./star build` ✅ 10/10 零错误
- E2E 单进程 ✅ stats 含 max_queue_depth + p99/p999
- CI Guard ✅ 20/20 .s.h 通过

---

## [v0.2.1] - 2026-04-11

### 架构清洁（"打扫屋子"）

- **TD1**: 删除 4 个 @deprecated .h 文件（799 行 ROS2 代码）
  - `modules/planning/navigation/refline.h` (63 行)
  - `modules/planning/motion/motion.h` (151 行)
  - `modules/setting/parameter/parameter.h` (527 行)
  - `modules/debug/debug_motion/debug_motion.h` (58 行)
- **messages/**: 删除整个 ROS2 消息定义目录（~40 文件，7 个 msg 包）
- **package.xml**: 删除 source tree 中 4 个 ROS2 包清单
- **CMakeLists.txt**: 标记剩余 5 个为 @LEGACY（含 basement 原有 1 个）
- **效果**: ROS2 代码引用从 ~50 行降至 **0 行**；@deprecated 文件从 4 个降至 **0 个**

### 验证

- `./star build` ✅ 14/14 零错误
- 端到端测试 ✅ motion component 正常运行
- CI Guard ✅ 20/20 .s.h 通过
- ROS2 残留扫描 ✅ **0 行**

---

## [v0.2] - 2026-04-11

### 新增

- **B1**: Manifest Schema 化 — 清理 ConfValue 死代码 96 行
- **B2**: RollingFileLogger — 按大小自动滚动的日志落盘
- **F1**: `star monitor` CLI 子命令 — 实时 transport 统计输出
- **F2**: Latency p99/p999 百分位统计
- **F3**: 统一日志格式 — ISO 8601 时间戳 + TID + 位置信息
- **F4**: IClock 抽象 — SteadyClock/SystemClock/SimulationClock
- **F5**: Supervisor 增强 — [Supervisor] 前缀 + supervisor_summary() 报告
- **F6**: HealthChecker — TopicHealthRule + 频率阈值检测

### 修复

- monitor() 方法缺少 options 参数导致构建失败
- std::ceil 缺少 `<cmath>` 头文件
- ShmMessageBus 未实现 declared_topics() 导致抽象类错误
- RollingFileLogger 创建后未集成到 runtime_factory.h
- CI guard header guard 路径不匹配
- pre-commit hook 路径解析错误
- CorrelationId 命名空间缺失
- unordered_map<CorrelationId> 缺少 hash 特化

### 文档更新

- basement_transport.md: 新增 §16-§18
- basement_core.md: 新增 §6-§8
- basement_progress.md: 更新成熟度 89%→93%，新增 §7

---

## [v0.1] - 2026-04-07

### 新增

- **M0**: Native-only 主路径全通（build/run/install/list/graph/doctor）
- **M1**: Schema IDL + codegen + codec + CI guard 全链路
- **M2**: Transport inproc/shm/routed + QoS + CorrelationId
- **M3**: Runtime choreography + FrameContext + SyncTrigger + auto_configure
- **P0-P5**: Blackboard 迁移 / codegen batch mode / compat / CI guard / async call / QoS

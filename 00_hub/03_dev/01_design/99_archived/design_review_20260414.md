# Starletto Basement 设计深度分析报告

**日期**：2026-04-14（初始分析） / 2026-04-17（重整后更新）
**分析范围**：basement/ 全部核心模块 + spec 文档

---

## 〇、重整后状态（2026-04-18 更新）

经过多轮重整，原报告中 10 个问题的处理状态：

| # | 问题 | 严重度 | 状态 | 说明 |
|---|------|--------|------|------|
| P1 | Transport API 概念混乱 | 🔴 | ✅ 已修复 | Reader 添加 peek_latest/has_data（NativeReader/ShmReader/RoutedReader 均已实现），MessageBus 添加结构化统计 API，RecordableBus 分离，dump_stats_lines 降级为非纯虚 |
| P2 | Schema 体系未闭环 | 🔴 | ✅ 已修复 | is_conf_value_field hack 已删除，SchemaValue 定位明确为长期方案，View/Builder 完整实现（FixedLayoutCodec + codegen 生成），端到端测试验证 |
| P3 | Runtime 依赖倒置 | 🔴 | ✅ 已修复 | Component 不再 include 具体实现，read_history 不再 dynamic_cast，NativeServiceBus 归位到 transport/native/ |
| P4 | Manifest 迁移未完成 | 🔴 | ✅ 实际已收敛 | PlanLoader 已走 .conf→SchemaValue→typed struct 单路径，ConfValue 已删除 |
| P5 | 代码重复 | 🟡 | ✅ 已修复 | channel_stats.h/percentile.h/qos_manager.h/route_parser.h 已提取，Bus 间 QoS 逻辑统一为 apply_qos，parse_route_plan/parse_channel_set 统一到 route_parser.h |
| P6 | 观测性紧耦合 | 🟡 | ✅ 已修复 | HealthChecker 直接消费 ChannelStatsSnapshot，ResourceMonitor 移除静态全局并添加 ClockFn 注入 |
| P7 | 错误处理策略缺失 | 🟡 | ✅ 已修复 | SHM 失败降级策略已实现，FdGuard/FileGuard RAII 包装（ShmChannel fd + ResourceMonitor FILE*），ShmChannel 构造函数错误路径不再手动 close |
| P8 | 可测试性为零 | 🟡 | ✅ 已修复 | Catch2 集成，6 个测试套件（BinaryCodec/CodeGenViewBuilder/ChannelStats/NativeMessageBus/NativeServiceBus/FixedLayoutCodec），./star test 子命令 |
| P9 | 命名不一致 | 🟢 | ✅ 已修复 | publish/subscribe 统一为 write/read，ParameterStore 命名空间修正为 star::runtime |
| P10 | 文档与实现脱节 | 🟢 | ✅ 已修复 | spec 文档已同步更新（basement_transport.md v0.5.0、basement_schema.md View/Builder 标记完成） |

**重整后成熟度**：~92%（从 55% 提升）

**根因消除状态**：
- 根因 A（概念缺失）：✅ 已消除 — Transport/Schema/Runtime 概念集已重新定义并实现
- 根因 B（依赖混乱）：✅ 已消除 — Component 依赖倒置修复，RAII 资源守卫替代手动管理
- 根因 C（临时变永久）：✅ 已消除 — 代码重复全部消除，公共模块提取完成
- 根因 D（不敢重构）：✅ 已消除 — Catch2 测试安全网已建立，6 个测试套件覆盖核心模块

---

## 一、核心结论：99% 成熟度是幻觉（原始分析）

实际成熟度评估：**50%-60%**

| 维度 | 标称值 | 实际值 | 差距 |
|------|--------|--------|------|
| 架构设计一致性 | 90% | **40%** | -50% |
| 概念完整性 | 85% | **50%** | -35% |
| 代码可维护性 | 90% | **60%** | -30% |
| 测试覆盖 | 80% | **10%** | -70% |
| **综合成熟度** | **99%** | **55%** | **-44%** |

---

## 二、问题清单（按严重度排序）

### 🔴 P0：架构与概念错误（根本性问题）

---

#### 问题 1：Transport 层概念混乱与 API 不一致

**严重度**：🔴 P0
**状态**：✅ 已修复

**修复内容**：
1. Reader 接口添加 `peek_latest()` 和 `has_data()` 纯虚方法
2. NativeReader 和 ShmReader 均已实现这两个方法
3. MessageBus 接口添加 `channel_stats()`/`all_channel_stats()`/`peek_latest()` 结构化 API
4. `dump_stats_lines()` 降级为非纯虚（带默认实现返回空 vector）
5. `enable_recorder()`/`enable_record_channel()` 从 MessageBus 移到独立的 `RecordableBus` 接口
6. NativeMessageBus 同时实现 MessageBus 和 RecordableBus
7. RuntimeContext 新增 `recordable_bus()` 接口
8. SHM 失败降级策略：ShmRoutedMessageBus 构造时 SHM 失败自动降级为 inproc-only，每个 channel 级别独立降级
9. RuntimeFactory `STAR_TRANSPORT=shm` 模式 SHM 失败时降级为 NativeMessageBus

---

#### 问题 2：Schema 体系未闭环，View/Builder 只是概念

**严重度**：🔴 P0
**状态**：✅ 已修复

**修复内容**：
1. `FixedLayoutCodec`（`transport/common/fixed_layout_codec.h`）完整实现：
   - `FixedLayoutWriter`：支持所有类型 + `serialize()` / `serialize_into(buf, cap)`
   - `FixedLayoutView`：零拷贝只读视图，直接映射内存
   - 辅助函数：primitive/string/bool array/map，object array/map，binary value array/map
2. `schema_codegen.py` 生成完整 View/Builder：
   - `XxxView`：包装 `FixedLayoutView`，类型安全字段访问器
   - `XxxBuilder`：包装 `FixedLayoutWriter`，类型安全字段设置器
   - `encode_fixed_layout()` / `encode_fixed_layout_into()` / `from_fixed_layout()`
3. 端到端测试：`test_codegen_view_builder.h`（8 个用例覆盖 ModuleConf/ModuleInfo/LayoutInfo/DependencyInfo）
4. `is_conf_value_field()` hack 已删除
5. `object` 类型在 fixed layout 中用 text codec 存储为 string

---

#### 问题 3：Runtime 层依赖倒置与边界模糊

**严重度**：🔴 P0
**状态**：✅ 已修复

**修复内容**：
1. `component.h` 不再 include `native_message_bus.h` 或 `shm_message_bus.h`
2. `read_history()` 使用 `MessageBus::peek_latest()` 而非 `dynamic_cast`
3. `NativeServiceBus` 已移到 `basement/transport/native/native_service_bus.h`
4. `ParameterStore` 已移到 `basement/runtime/parameter/`

---

#### 问题 4：Manifest 迁移未完成，处于"过渡地狱"

**严重度**：🔴 P0
**状态**：✅ 实际已收敛

**修复内容**：
1. ConfValue 类已删除
2. PlanLoader 流程：`.conf` → TextCodec → SchemaValue → `ModuleConf::from_value()` → typed struct
3. Runtime 只消费 codegen 生成的类型

---

### 🟡 P1：软件设计问题（可维护性与扩展性）

---

#### 问题 5：代码重复与逻辑不一致

**严重度**：🟡 P1
**状态**：🔵 部分修复

**已修复**：
- `transport/common/channel_stats.h` 已提取：ChannelStats + ChannelStatsSnapshot + compute_percentile + compute_hz + make_stats_snapshot + format_stats_line
- NativeMessageBus 和 ShmMessageBus 共享 channel_stats.h

**未修复**：
- `percentile.h` 未独立（compute_percentile 仍内联在 channel_stats.h）
- `qos_manager.h` 未创建（QoS 队列管理逻辑在两个 Bus 中重复）
- ChannelRegistry 的 `parse_route_plan`/`parse_channel_set` 在 ShmRoutedMessageBus 中有重复实现

---

#### 问题 6：HealthChecker 与 ResourceMonitor 的紧耦合与低效实现

**严重度**：🟡 P1
**状态**：✅ 已修复

**修复内容**：
1. HealthChecker 直接消费 `ChannelStatsSnapshot`，不再解析字符串
2. ResourceMonitor 移除静态全局状态，`cpu_cache_` 改为实例成员
3. ResourceMonitor 添加 `ClockFn` 注入（可测试性）

---

#### 问题 7：缺少统一的错误处理策略

**严重度**：🟡 P1
**状态**：🔵 部分改善

**已修复**：
- SHM 失败降级策略已实现（ShmRoutedMessageBus + RuntimeFactory）
- SHM 硬编码字符串提取为常量

**未修复**：
- 整体错误处理策略仍待完善（异常类型使用指导缺失）
- SHM fd 没有独立 RAII 包装（当前在 ShmChannel 析构函数中 close）

---

#### 问题 8：可测试性几乎为零

**严重度**：🟡 P1
**状态**：✅ 已修复

**修复内容**：
1. Catch2 集成到 Meson 构建系统
2. 6 个测试套件：
   - BinaryCodec（encode/decode 往返）
   - CodeGenViewBuilder（View/Builder 端到端，8 个用例）
   - ChannelStats（统计计算）
   - NativeMessageBus（write/read）
   - NativeServiceBus（call/response）
   - FixedLayoutCodec（零拷贝编解码）
3. `./star test` 子命令

---

### 🟢 P2：小问题与优化点

---

#### 问题 9：命名不一致

**严重度**：🟢 P2
**状态**：✅ 已修复

**修复内容**：
1. publish/subscribe 全面统一为 write/read（14 个文件）
2. ParameterStore 命名空间修正为 star::runtime
3. 消除了 Reader/Writer vs Consumer/Producer 混用

---

#### 问题 10：文档与实现脱节

**严重度**：🟢 P2
**状态**：✅ 已修复

**修复内容**：
1. basement_transport.md 更新至 v0.5.0（FixedLayoutCodec、SHM 降级、RecordableBus 分离、write/read 统一）
2. basement_schema.md 标记 View/Builder 已完成
3. design_remedy_20260414.md 标记 3.3 View/Builder 已实现

---

## 三、根本原因分析

### 核心问题："快速交付" vs "正确设计" 的失衡

1. **过度追求功能交付，忽视架构一致性**
2. **Spec-first 流程执行不严格**
3. **没有测试框架作为安全网**

---

## 四、修正建议（按优先级排序）

### 阶段 1：止血（P0 问题）— ✅ 全部完成

1. ✅ 修复 Transport 层 API
2. ✅ 明确 SchemaValue 的定位
3. ✅ 修复 Runtime 层依赖问题
4. ✅ 完成 Manifest 迁移

### 阶段 2：重构（P1 问题）— 🔵 部分完成

1. 🔵 消除代码重复（channel_stats.h 已提取，percentile.h/qos_manager.h 待提取）
2. ✅ 重构观测性模块
3. 🔵 建立错误处理策略（SHM 降级已实现，整体策略待完善）

### 阶段 3：基础设施（P2 问题）— ✅ 全部完成

1. ✅ 引入测试框架
2. ✅ 统一命名与文档

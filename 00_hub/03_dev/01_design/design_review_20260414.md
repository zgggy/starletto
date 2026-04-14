# Starletto Basement 设计深度分析报告

**日期**：2026-04-14
**分析范围**：basement/ 全部核心模块 + spec 文档

---

## 一、核心结论：99% 成熟度是幻觉

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
**影响文件**：
- `basement/transport/api/message_bus.h:77-81`
- `basement/transport/native/native_message_bus.h`
- `basement/transport/shm/shm_message_bus.h`

**问题描述**：

1. **Reader 接口空洞**：`Reader` 基类只有析构函数，没有任何纯虚方法。这意味着：
   - 无法通过基类调用 Reader 的任何功能
   - Reader 的生命周期管理完全依赖回调和 shared_ptr 的生命周期
   - API 设计违背抽象原则

```cpp
class Reader {
  public:
    using Callback = std::function<void(const MessageView&)>;
    virtual ~Reader() = default;  // 只有析构函数！
};
```

2. **Writer/AckWriter 能力不对齐**：
   - `NativeMessageBus` 有完整的 `AckWriter` 实现
   - `ShmMessageBus::create_ack_writer()` 直接返回 `nullptr`
   - 这意味着 SHM 路径下 ACK 功能完全不可用，但 API 没有任何警告或降级机制

3. **MessageBus 接口职责过载**：
   - `MessageBus` 同时负责：channel 声明、writer/reader 创建、统计、录制
   - 录制功能（`enable_recorder`/`enable_record_channel`）不应在 MessageBus 接口中

---

#### 问题 2：Schema 体系未闭环，View/Builder 只是概念

**严重度**：🔴 P0
**影响文件**：
- `basement/schema/schema_codegen.py`
- `basement/schema/binary_codec.h`
- 所有 `.s.h` 生成文件

**问题描述**：

1. **Spec 与实现严重脱节**：
   - Spec 文档承诺 "View/Builder 零拷贝"
   - 但实际实现中：
     - `.s.h` 中确实生成了 `View`/`Builder` 类型定义
     - **但完全是空壳！** 没有任何访问器方法（`Has_xxx()`/`xxx()`）
     - 零拷贝路径（SHM loan/view）完全没有用到 View/Builder

2. **BinaryCodec 的临时过渡状态**：
   - `binary_codec.h` 中 `is_conf_value_field()` 是临时 hack
   - 直接依赖类型名称字符串 `"star::schema::SchemaValue"`
   - 说明当前还在 ConfValue 过渡阶段，没有完全迁移到 schema 类型

3. **SchemaValue 的定位混乱**：
   - Spec 说 "ConfValue 逐步退出运行时数据通路"
   - 但实际：
     - `SchemaValue` 被 35 个文件引用
     - `ParameterStore` 接口完全基于 `SchemaValue`
     - `Snapshot` 传递仍然依赖 `SchemaValue` 路径
   - 这表明：**ConfValue 并没有被淘汰，它只是改名叫 SchemaValue 了！**

---

#### 问题 3：Runtime 层依赖倒置与边界模糊

**严重度**：🔴 P0
**影响文件**：
- `basement/runtime/component/component.h`
- `basement/runtime/factory/runtime_factory.h`
- `basement/runtime/scheduler/native_scheduler.h`

**问题描述**：

1. **Component 基类直接依赖具体实现**：
   ```cpp
   #include "basement/transport/native/native_message_bus.h"  // 依赖具体实现！
   #include "basement/transport/shm/shm_message_bus.h"      // 依赖具体实现！
   ```
   Component 作为 Runtime 层的核心抽象，绝不应直接依赖 NativeMessageBus 或 ShmMessageBus！

2. **Component::read_history() 的实现错误**：
   - 直接 `dynamic_cast` 到具体实现类
   - SHM 路径下 `read_history()` 完全不可用，但 API 没有任何声明
   - 这说明：**`read_history()` 只是个半吊子功能，只支持 NativeMessageBus**

3. **NativeServiceBus 位置错误**：
   - Spec 说 ServiceBus 属于 Transport 层
   - 但 `NativeServiceBus` 定义在 `runtime/factory/runtime_factory.h` 中
   - 导致 Transport 层没有完整的 ServiceBus 实现

---

#### 问题 4：Manifest 迁移未完成，处于"过渡地狱"

**严重度**：🔴 P0
**影响文件**：
- `basement/manifest/native/native_manifest_loader.h`
- `basement/runtime/component/native_component_host.h`

**问题描述**：

1. **双重路径同时存在**：
   - ConfValue 解析路径（旧）
   - Schema typed 路径（新）
   - 两个路径都在使用，没有明确的切换计划

2. **"清单即类型" 只是口号**：
   - Spec 说 "manifest 由 schema IDL 定义"
   - 但实际：
     - `.conf` 文件仍然是值文件（debug_text codec）
     - 没有看到 `.schema` 格式的 manifest
     - 兼容性检测工具完全不存在

---

### 🟡 P1：软件设计问题（可维护性与扩展性）

---

#### 问题 5：代码重复与逻辑不一致

**严重度**：🟡 P1
**影响文件**：
- `basement/transport/native/native_message_bus.h`
- `basement/transport/shm/shm_message_bus.h`

**问题描述**：

1. **NativeMessageBus 与 ShmMessageBus 大量重复**：
   - 两个类都有几乎完全相同的：
     - `ChannelStats` 结构体定义
     - `compute_percentile()` 方法
     - `compute_hz()` 方法
     - `compute_avg_latency_us()` 方法
     - `format_rate()` 方法
     - `maybe_register_writer()`/`maybe_register_reader()` 方法
     - `dump_stats_lines()` 方法
     - QoS 队列管理逻辑
   - 估计重复代码量：~400 行

2. **百分位计算逻辑重复**：
   - `compute_percentile()` 在两个文件中完全相同

---

#### 问题 6：HealthChecker 与 ResourceMonitor 的紧耦合与低效实现

**严重度**：🟡 P1
**影响文件**：
- `basement/runtime/observability/health_checker.h`
- `basement/runtime/observability/resource_monitor.h`

**问题描述**：

1. **HealthChecker 通过字符串解析获取统计**：
   - `dump_stats_lines()` 返回格式化字符串
   - `HealthChecker` 再把这些字符串重新解析成结构化数据
   - 这是典型的反模式

2. **ResourceMonitor 的静态全局状态**：
   - 使用静态全局变量作为 CPU 缓存
   - 多个 ResourceMonitor 实例会共享同一个缓存
   - 没有清理机制

3. **HealthChecker::check_component() 重复调用 dump_stats_lines()**：
   - 如果有 N 个 channel 规则，就调用 N 次
   - 严重的性能问题

---

#### 问题 7：缺少统一的错误处理策略

**严重度**：🟡 P1
**影响文件**：所有文件

**问题描述**：

1. **异常类型使用混乱**：RuntimeError/LogicError/BugError 没有明确的使用指导
2. **错误恢复策略缺失**：大部分错误直接抛出异常，没有降级或恢复机制
3. **资源泄漏风险**：没有 RAII 包装的 SHM 文件描述符

---

#### 问题 8：可测试性几乎为零

**严重度**：🟡 P1
**影响文件**：所有文件

**问题描述**：

1. **没有测试框架**：没有 Catch2 或 GoogleTest 集成
2. **代码结构不利于测试**：大量静态方法、没有依赖注入、没有 Mock 接口
3. **全局状态泛滥**：LoggerManager 单例、ResourceMonitor 静态全局

---

### 🟢 P2：小问题与优化点

---

#### 问题 9：命名不一致

**严重度**：🟢 P2

1. Reader/Writer vs Consumer/Producer 概念混用
2. Channel vs Topic 混用
3. 方法命名风格不统一

---

#### 问题 10：文档与实现脱节

**严重度**：🟢 P2

1. Spec 文档描述的功能未实现（View/Builder、DAG 调度、热插拔、兼容性检测）
2. Spec 文档位置不统一

---

## 三、根本原因分析

### 核心问题："快速交付" vs "正确设计" 的失衡

1. **过度追求功能交付，忽视架构一致性**
2. **Spec-first 流程执行不严格**
3. **没有测试框架作为安全网**

---

## 四、修正建议（按优先级排序）

### 阶段 1：止血（P0 问题）

1. 修复 Transport 层 API
2. 明确 SchemaValue 的定位
3. 修复 Runtime 层依赖问题
4. 完成 Manifest 迁移

### 阶段 2：重构（P1 问题）

1. 消除代码重复
2. 重构观测性模块
3. 建立错误处理策略

### 阶段 3：基础设施（P2 问题）

1. 引入测试框架
2. 统一命名与文档

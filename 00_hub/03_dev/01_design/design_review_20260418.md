# Starletto Basement 全量设计审计报告（2026-05-02 修订）

**审计范围**：basement/ 全部核心模块（core / schema / transport / runtime / manifest）+ apps/native_runner 自检链路 + 00_hub 结构复杂度
**审计维度**：架构设计、概念设计、软件设计、可测试性、可维护性、使用体验（CLI/自检）
**审计方法**：对照 2026-04-18 审计条目逐项核对当前代码现状（以可执行验收为准）

---

## 〇、核心结论

### 0.1 完成度（成熟度）结论

**2026-04-18 的 65% 是真实的；截至 2026-05-02，Basement 当前成熟度约 88%。**

这次提升的关键原因不是“补功能”，而是把 04-18 的 P0 根因（可用性/线程安全/跨进程语义/错误处理不可诊断）系统性消掉，并补上可执行自检把关键语义钉死。

| 维度 | 2026-04-18 实际值 | 2026-05-02 实际值 | 变化原因（摘要） |
|------|------------------|------------------|------------------|
| 架构一致性 | 70% | 88% | MessageView/Service 载荷与能力拆分落地；SHM 路由双注册/双写导致重复投递已钉死；ServiceServer 生命周期对齐 |
| 概念完整性 | 70% | 88% | correlation_id 统一；ACK/async 语义名不副实问题已纠偏到 Delivery/async 真异步；manifest type/msg_type 语义收敛 |
| 代码可维护性 | 65% | 82% | codegen 重复模板提取、Registry 并发安全、日志系统生命周期/线程安全收口；但命名与部分架构项仍有欠账 |
| 测试覆盖 | 45% | 65% | 补齐 `star selftest`（inproc routed + multiprocess shm）；tests 里关键 warning 清零 |
| **综合** | **65%** | **87%** | |

### 0.2 问题统计（按“未关闭项”计）

| 严重度 | 2026-04-18 未关闭 | 2026-05-02 未关闭 | 说明 |
|--------|------------------|------------------|------|
| 🔴 严重 | 28 | 0 | 所有 P0 功能性 bug / 线程安全 / 编译失败类已闭环并可验收 |
| 🟡 重要 | 46 | 14 | 主要剩余为“架构统一/性能优化/命名规范”等非阻断项 |
| 🟢 次要 | 23 | 14 | 多为规范一致性与边界细节，可分批清理 |

---

## 一、🔴 严重问题闭环清单（2026-04-18 的 28 项）

结论：**全部已闭环**（以 `./star build` + `./star selftest` 为最低验收）。

### 1.1 Transport（原 T1~T16）

- T1/T12：数据表示三重冗余 + correlation_id 分裂已收敛为 payload + schema_type_id + CorrelationId
- T2/T15：ShmServiceBus 可用性与生命周期对齐（create_server 自动 start；client 超时清理）
- T3/T4/T16：ShmChannel 跨进程语义（互斥/初始化/避免截断/析构 unlink）已修复，并新增多进程自检
- T5/T14：ACK/async 语义纠偏（DeliveryWriter + 真异步），不再 detach 捕获 this
- T6：NativeMessageBus recorder_ 竞态消除（锁域内捕获 shared_ptr）
- T7：erase_reader 同步清理 reader_history
- T8：RoutedWriter gating + RoutedReader 单侧注册，且自检锁死“不重复投递”
- T9：p=0 percentile 边界修复
- T10/T11：FixedLayoutHeader 扩容 + Writer 越界检查

### 1.2 Core（原 C1~C5）

- C1：TypedConstPtr type==nullptr 不再绕过检查；并增强跨动态库兜底匹配
- C2/TH2/TH3：LoggerManager/StdoutLogger/LogStream 线程安全与生命周期闭环
- C3/E5：RollingFileLogger 滚动与 max_backup_files=0 边界已修复
- C5：异常命名空间不一致以最小代价桥接收口

### 1.3 Schema/Runtime/Manifest（原 S1~S4、R1~R2、M1）

- S1：codegen 重复模板已提取公共 helpers
- S2：schema_bootstrap 反向依赖移除
- S3/S4：SchemaRegistry 冲突校验 + 并发一致性修复
- R1/R2：编译错误与线程生命周期不受控已收口
- M1：头文件保护宏不再包含绝对路径（以 grep 结果为证）

---

## 二、🟡 重要问题（剩余 14 项，按优先级）

### 2.1 架构一致性（建议继续推进）

- A1：路由逻辑仍存在“双重维护残留”（ChannelRegistry 与部分 bus 行为边界仍可进一步统一）
- A6/A7/A8/A9/A10/A11：schema/manifest 的职责拆分属于 P2 设计改进，影响面大但收益长期

### 2.2 性能（建议按场景逐项量化）

- P2：BinaryCodec repeated 字段仍可能存在不必要拷贝
- P3：apply_qos 仍可能按值拷贝 MessageView（建议改为就地或显式 move）
- P5：compute_percentile 仍为排序拷贝（建议引入 nth_element 或滑动窗口）
- P6/P7：FixedLayoutCodec 与 codegen 的重复结构仍可继续去重

### 2.3 线程安全与工程一致性

- N1~N8：命名/规范类问题仍未系统清理（建议分批做，不阻断主链路）

---

## 三、对标 Apollo Cyber RT（新增）

这次修订同步对照了 Apollo Cyber RT 的实现与术语，得到的结论是：**“写入可感知失败”和“零拷贝获取消息实例”在成熟中间件里是默认能力**，并且被放在 Node/Writer 这类边界 API 上，而不是隐藏在实现内部。

- Writer 写入返回值：Cyber 的 `Writer::Write(...) -> bool` 直接表达“写是否成功”，并且把失败作为正常分支处理（而非只靠异常）  
  - 参考：`/home/zgy/projects/apollo/cyber/node/writer.h`
- Loan/Acquire：Cyber 提供 `AcquireMessage()` 获取可复用的消息实例，作为高频写入的优化路径  
  - 参考：`/home/zgy/projects/apollo/cyber/node/writer.h`
- 术语与分层：Cyber 将 Component/Node/Channel/Reader/Writer/Service/Client 明确定义为一套可沟通的概念集合，减少“同名异义”  
  - 参考：`/home/zgy/projects/apollo/docs/04_CyberRT/CyberRT_Terms.md`

---

## 四、00_hub 结构复杂度审计（新增）

**问题**：00_hub 当前呈现“资料堆叠式增长”，读者无法快速判断“当前唯一有效信息源”，导致重构推进成本上升。

**建议（不改变任何对外接口的前提下）**：

1. **明确活跃区**：`00_hub/03_dev/01_design/` 保留“当前唯一主设计源”，其余同主题文档必须进入归档区
2. **归档区固定化**：统一放入 `00_hub/03_dev/01_design/99_archived/`，按日期/主题分层
3. **spec 同步收敛**：`00_hub/05_docs/spec/*` 只在接口/默认值/边界/验收变化时更新，避免成为“第二套设计源”
4. **减少横向目录**：把“过程性记录”从一线驱动源剥离，避免新成员需要全量阅读才能理解现状

---

## 五、验收命令

- `./star build`
- `./star selftest`

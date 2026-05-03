# Starletto Basement 全量设计重整方案（2026-05-02 修订）

**日期**：2026-05-02
**目标**：简洁、强大、优美
**前置文档**：[全量设计审计报告](design_review_20260418.md)

---

## 一、当前阶段结论（对 2026-04-18 的重整计划复盘）

### 1.1 已完成的“必修课”

- P0 已清零：可用性/线程安全/编译失败/跨进程语义/关键错误处理可诊断性已闭环
- `star selftest` 已具备两条“系统级锁死语义”的自检：
  - routed 同进程不重复投递
  - SHM 多进程并发写读不出现真实损坏（并区分“覆盖”与“损坏”）

### 1.2 仍需推进的“架构与质量收口”

剩余问题集中在：路由统一、schema/manifest 职责拆分、性能优化与规范一致性。

---

## 二、下一阶段修复优先级（2026-05-02 起）

### P1：架构一致性收口（优先）

| # | 问题 | 目标 | 验收 |
|---|------|------|------|
| A1 | 路由逻辑统一 | 以 ChannelRegistry 为单一事实源，消除“同一判断分散在多处”的残留 | routed 行为只由一处配置驱动，且 `./star selftest` 不回退 |

### P1.5：对标 Apollo Cyber RT（新增）

- Cyber 的 Writer API 默认返回 `bool` 表达“写是否成功”，并提供 `AcquireMessage()` 作为高频写入的优化入口；这些能力值得作为边界 API 的默认形态  
  - 参考：`/home/zgy/projects/apollo/cyber/node/writer.h`

### P2：设计改进（分批推进，不阻断主链路）

| # | 问题 | 建议动作 |
|---|------|----------|
| A6/A7 | BinaryCodec 可扩展性与一致性 | 引入可注入策略对象；清理“二进制里嵌文本回退”的路径 |
| A8 | FieldDescriptor 职责过重 | 拆分为类型系统/验证/序列化元数据三个维度 |
| A9 | NativeSchemaLoader 双格式解析 | 新旧格式拆成两个 loader，由入口层判别与路由 |
| A10 | Port 职责过重 | manifest 端点拆分为 Reader/Writer/Command descriptor |
| A11 | channel_stats 重复 | 以 transport 层统计为主，runtime 层只做聚合与展示 |

### P3：性能优化（先量化，再改）

| # | 问题 | 建议动作 |
|---|------|----------|
| P2 | repeated 字段拷贝 | 改为引用/指针访问或预展开 descriptor，避免 per-element copy |
| P3 | apply_qos 拷贝 MessageView | 改为就地策略（in-place）或 move-only 快路径 |
| P5 | compute_percentile 排序拷贝 | 用 nth_element 或维护滑动窗口摘要 |
| P6/P7 | FixedLayout/Codegen 重复 | 提取公共构造函数，减少生成代码体积 |

### P4：线程安全与一致性（尾巴清理）

| # | 问题 | 建议动作 |
|---|------|----------|
| N1~N8 | 命名与规范 | 分批执行：先“可自动化检查”的项，再做“影响接口”的项 |

---

## 三、00_hub 优化（新增）

目标：让 00_hub 从“资料堆”变成“可执行的单一事实源 + 可追溯归档”。

### 3.1 已执行

- 历史审计/重整文档已归档到 `00_hub/03_dev/01_design/99_archived/`

### 3.2 建议执行（不引入新文档的前提下）

- 只保留 `00_hub/03_dev/01_design/` 下 2 份“当前主设计源”（review/remedy），其余一律归档
- `00_hub/05_docs/spec/*` 只同步接口/默认值/边界/验收变化，避免与主设计源竞争
- 逐步削减“过程性记录”对开发的强制依赖：让代码与验收命令成为最强证据

---

## 四、验收命令（最低标准）

- `./star build`
- `./star selftest`

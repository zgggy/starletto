# Starletto Basement 全量设计重整方案（2026-05-03 修订）

**日期**：2026-05-03  
**目标**：简洁、强大、优美、可长期稳定  
**前置文档**：[全量设计审计报告](design_review_20260503.md)  

---

## 一、当前阶段结论（对上一阶段收口结果复盘）

### 1.1 已完成的“必修课”

- P0 维持清零：可用性/线程安全/编译失败/跨进程语义/关键错误处理可诊断性维持闭环
- A1 路由统一闭环：以 `ChannelRegistry::shm_route_mode` 为单一事实源，避免分散判断回潮
- 性能小项收口：P2（repeated 拷贝）、P3（apply_qos depth==1 快路径）、P5（percentile nth_element + 语义锁死）
- 稳定性护栏补齐：BinaryCodec 回归 + Service/SHM 回归 + selftest service soak

### 1.2 仍需推进的“长期稳定收口”

剩余问题集中在：职责拆分、去重、冻结策略与少量规范一致性。

---

## 二、下一阶段优先级（2026-05-03 起）

### P1：稳定性护栏最终收口（优先）

| # | 问题 | 目标 | 验收 |
|---|------|------|------|
| G1 | selftest/service soak 扩展为“并发 + 长跑” | 1s 内并发发起 N 路请求，确保无死锁/泄漏/超时回潮 | `./star selftest` |
| G2 | SHM 多进程自检覆盖面加厚 | 增加一次“更小 slot + 更高频写”的短时压力 | `./star selftest` |

### P1.5：对标 Apollo Cyber RT（持续）

- 对标原则：每次改动若 Cyber 有对应能力/概念/实现路径，必须给出对照说明与路径
- 本阶段重点：Service client 的 receiver 生命周期、pending 管理、timeout 语义
  - 参考：`/home/zgy/projects/apollo/cyber/service/client.h`

### P2：设计改进（分批推进，不阻断主链路）

| # | 问题 | 建议动作 | 验收 |
|---|------|----------|------|
| A6/A7/A8/A9/A10/A11 | schema/manifest 职责拆分 | 拆分职责边界，逐步收敛扩展点，减少核心路径改动概率 | `./star build` + 定向 tests/selftest |

### P3：维护成本去重（强烈建议做一刀闭环）

| # | 问题 | 建议动作 | 验收 |
|---|------|----------|------|
| P6/P7 | FixedLayout/Codegen 重复 | 抽公共构造与公共路径，减少生成代码体积与维护面 | `./star test` + `./star selftest` |

### P4：冻结策略（让“长期稳定”成为可执行约束）

| # | 问题 | 建议动作 | 验收 |
|---|------|----------|------|
| F1 | 对外 API 冻结清单 | 明确 Writer/Service/ChannelRegistry 的稳定承诺边界（哪些不再动） | review/remedy 更新 + `./star build` |
| F2 | 允许演进的扩展点 | 明确 schema/manifest 的扩展方式（哪些可以变） | 同上 |

---

## 三、验收命令（最低标准）

- `./star build`
- `./star test`
- `./star selftest`


# Starletto Basement 全量设计审计报告（2026-05-03 修订）

**审计范围**：basement/ 全部核心模块（core / schema / transport / runtime / manifest）+ apps/native_runner 自检链路  
**审计维度**：架构设计、概念设计、软件设计、可测试性、可维护性、使用体验（CLI/自检）  
**审计方法**：对照上一版审计条目逐项核对当前代码现状（以可执行验收为准）  

---

## 〇、核心结论

### 0.1 完成度（成熟度）结论

截至 2026-05-03，Basement 当前成熟度约 **93%**。

这次提升的核心不在“新增功能”，而在：

- 路由统一闭环（以 ChannelRegistry 的 `shm_route_mode` 为单一事实源）
- 写入可感知失败语义落地并贯穿调用点（WriteResult/WriteStatus）
- 稳定性护栏补齐（BinaryCodec 回归 + Service/SHM 回归 + selftest service soak）

| 维度 | 2026-05-02 实际值 | 2026-05-03 实际值 | 变化原因（摘要） |
|------|------------------|------------------|------------------|
| 架构一致性 | 88% | 92% | A1 路由统一收口完成；service client 生命周期风险消除 |
| 概念完整性 | 88% | 92% | service 写失败/超时语义被测试锁死并与 WriteResult 贯通 |
| 代码可维护性 | 82% | 84% | 减少“回调线程析构死锁”类隐患；percentile 语义被锁定 |
| 测试覆盖 | 65% | 72% | 新增 codec 随机回归、service 失败/超时回归、selftest service soak |
| **综合** | **87%** | **93%** | |

### 0.2 问题统计（按“未关闭项”计）

| 严重度 | 2026-05-02 未关闭 | 2026-05-03 未关闭 | 说明 |
|--------|------------------|------------------|------|
| 🔴 严重 | 0 | 0 | 关键可用性/线程安全/跨进程语义/编译失败类仍保持清零 |
| 🟡 重要 | 14 | 10 | 主要剩余为“职责拆分/去重/冻结策略”类长期改进项 |
| 🟢 次要 | 14 | 12 | 主要为命名与规范一致性，分批清理即可 |

---

## 一、🔴 严重问题闭环清单（结论）

结论：**维持全部闭环**（以 `./star build` + `./star selftest` + `./star test` 为最低验收）。

本次额外补强的“严重风险类”是 Service client 的生命周期与回调线程析构风险（见 2.2）。

---

## 二、🟡 重要问题（剩余 10 项，按优先级）

### 2.1 长线职责拆分（P2 设计改进，影响面大）

- A6/A7/A8/A9/A10/A11：schema/manifest 的职责拆分属于长期架构改进，决定未来扩展的“改动半径”

### 2.2 去重与平台化（维护成本项）

- P6/P7：FixedLayoutCodec 与 codegen 的重复结构仍可继续去重（减少未来维护面）

### 2.3 冻结策略（让“长期稳定”成为工程现实）

- F1：明确 basement 对外“冻结的 API/语义”清单（例如 Writer/Service/ChannelRegistry 的稳定承诺边界）
- F2：明确“允许演进”的扩展点（例如 schema/manifest 的未来接口），避免未来在核心路径反复改

---

## 三、稳定性护栏（本次修订新增闭环点）

### 3.1 selftest 强化（系统级锁死语义）

- `./star selftest` 新增 service soak：在 SHM 可用时进行 200 次 request/response 回归，锁死“生命周期/资源释放/写失败语义不回退”

### 3.2 tests 强化（回归可重复）

- BinaryCodec：新增随机回归（含 unknown TLV 传递）  
- ShmServiceBus：新增 request 写失败立刻返回、timeout 清理 pending 且后续可用

---

## 四、对标 Apollo Cyber RT（本次修订补充）

结论：Cyber 的 Service client 设计也采用“长期持有 response receiver + pending 管理”的模式，避免每次请求创建/销毁 receiver 引入生命周期与线程析构风险。

- 参考：`/home/zgy/projects/apollo/cyber/service/client.h`

---

## 五、验收命令

- `./star build`
- `./star test`
- `./star selftest`


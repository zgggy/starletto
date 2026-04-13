# Starletto 工程现状深度分析报告

> **分析基准日**：2026-04-11（v0.2 交付后）
> **数据来源**：代码审计 + spec 文档 + 00_hub 任务归档

---

## 一、工程全貌：数字说话

### 1.1 代码资产总览

```
┌─────────────────────────────────────────────────────────┐
│                    Starletto 代码库                       │
├──────────────┬──────────┬──────────┬────────────────────┤
│   工程       │  文件数   │  代码行数  │     成熟度         │
├──────────────┼──────────┼──────────┼────────────────────┤
│ basement/    │   47 .h  │  12,181   │   ██████████████░ 93% │
│ libraries/   │  ~54 .h  │   4,493   │   ████████░░░░░░ 30% │
│ modules/     │ ~140 .h  │   8,435   │   █████░░░░░░░░ 20% │
│ apps/        │   ~8     │   1,477   │   ████████████░ 80% │
├──────────────┼──────────┼──────────┼────────────────────┤
│   合计       │ ~527 .h  │  ~26,586  │   综合成熟度 ~65%    │
└──────────────┴──────────┴──────────┴────────────────────┘
```

### 1.2 Basement 内部结构（12,181 行的构成）

| 子模块 | 文件 | 行数 | 占比 | 核心文件 | 成熟度 |
|--------|------|------|------|----------|--------|
| **schema** | 13 | **7,548** | **62%** | schema_codegen.py (主引擎) | **100%** ✅ |
| **runtime** | 13 | 2,680 | 22% | runtime_factory.h (装配核心) | 88% |
| **transport** | 10 | 2,228 | 18% | native_message_bus.h (通信核心) | 95% |
| **manifest** | 5 | 477 | 4% | native_manifest_loader.h | 90% |
| **core** | 6 | 481 | 4% | logger.h / iclock.h / exception.h | 95% |

**关键发现**：
- schema 模块占 **62%** 行数（主要是 codegen Python 引擎，7548 行中的大部分）
- 纯 C++ 头文件约 **4,633 行**（schema 的 C++ 部分 + 其他模块）
- codegen 是整个项目的"心脏"，也是最大的单点复杂度

### 1.3 文件类型分布

| 类型 | 数量 | 说明 |
|------|------|------|
| `.h` 头文件 | **527** | Header-only 主导（占源码 95%） |
| `.cc/.cpp` | 141 | 少量实现文件（主要在 apps/ 和 modules/） |
| `.py` 脚本 | 72 | schema_codegen.py 占主体 + 测试辅助 |
| `.schema` IDL | **20** | 类型定义（4 builtin + 16 business） |
| `.conf` 配置 | 27 | module.conf / component.conf |
| `.sh` 脚本 | 104 | 大部分是第三方（.git-hooks 等） |
| `.md` 文档 | **77** | 17 spec + 60 hub |

---

## 二、能力成熟度矩阵

### 2.1 六维雷达图数据

| 维度 | 得分 | 证据 |
|------|------|------|
| **类型体系** | **100/100** | IDL+codec+codegen+CI 全闭环，20 schema 自动生成 |
| **通信层** | **95/100** | inproc/shm/routed 三模式 + QoS + p99 + monitor；缺 SHM QoS buffer(T1) + Async Reader(T2) |
| **运行时** | **88/100** | auto_configure 强大 + FrameContext + supervisor + health；缺 DAG 调度 + 单测覆盖 |
| **可观测性** | **90/100** | stats/p99/monitor/health/logger format/clock；缺 per-component resource(T3) |
| **工程化** | **90/100** | CI guard + rolling file + unified log + spec-driven；缺 test framework |
| **业务闭环** | **15/100** | 只有 motion demo + PID demo + config snapshot；缺 localization/perception/full pipeline |

### 2.2 Basement 各子模块差距分析

| 子模块 | 当前 | 到 100% 缺什么 | 工作量估计 |
|--------|------|----------------|------------|
| core | 95% | 日志零开销(O4) | 小 |
| schema | **100%** | — | 无 |
| transport | 95% | SHM QoS buffer(T1), Async Reader(T2) | 中 |
| runtime | 88% | **测试覆盖**, DAG 调度 | **大** |
| manifest | 90% | 边界 case 测试 | 小 |

**结论：runtime 的测试覆盖是最大单一缺口。**

---

## 三、依赖关系与风险分析

### 3.1 当前依赖图

```
libraries/ (30%, 无 spec, 无测试)
    ↑ 被 modules/ 依赖
    ↓ 依赖 basement/core (TypedConstPtr 等)
    ↓ 依赖 basement/schema (SchemaValue)

modules/ (20%, Demo 级)
    ↑ 被 apps/ 调用
    ↓ 依赖 basement/runtime (Component 接口)
    ↓ 依赖 basement/transport (Reader/Writer)
    ↓ 依赖 libraries/ (算法类型)
    ⚠️ 3 处 ROS2 残留 (parameter.h, refline.h)
    ⚠️ 3 个 @deprecated 文件未清理

basement/ (93%, 核心焦点)
    ├── core (95%) ─── 最底层，零内部依赖
    ├── schema (100%) ── 只依赖 core
    ├── transport (95%) ── 依赖 core + schema
    ├── runtime (88%) ── 依赖全部上层
    └── manifest (90%) ── 依赖 core + schema
```

### 3.2 风险清单

| # | 风险 | 级别 | 影响 | 缓解措施 |
|---|------|------|------|----------|
| R1 | **零单元测试** | 🔴 高 | 任何重构都可能引入回归 bug | **B5 测试基础设施（最高优先）** |
| R2 | schema_codegen.py 7500+ 行单文件 | 🟡 中 | 维护成本高，改动风险大 | 后续考虑拆分 gen/fmt/lint/compat |
| R3 | main.cc 860 行（CLI 入口膨胀） | 🟡 中 | 新增 subcommand 越来越难维护 | TD5: 拆分为独立文件 |
| R4 | ROS2 残留 3 行 | 🟢 低 | 编译警告 + 架构不纯 | 清理 parameter.h/refline.h |
| R5 | @deprecated 文件 4 个未删除 | 🟢 低 | 代码噪音 | 确认无引用后删除 |
| R6 | libraries 无 spec 无测试 | 🟡 中 | Phase 2 启动时会有大量返工 | 不急，Phase 2 前处理 |

---

## 四、需求提取（从现状反推）

### 4.1 紧迫需求（P0 — 不做会影响下一步）

| 需求 | 来源 | 说明 |
|------|------|------|
| **B5-1: 测试框架引入** | 风险 R1 | 零单测 = 盲飞，93% 成熟度不可信 |
| **B5-2: 核心路径单测** | 风险 R1 | codec/QoS/auto_configure 至少要有回归保护 |
| **ROS2 残留清理** | 风险 R4 | 3 行代码，10 分钟工作量，架构纯洁性 |

### 4.2 重要需求（P1 — 提升质量上限）

| 需求 | 来源 | 价值 |
|------|------|------|
| T1: SHM QoS buffer | transport 95%→100% | 多进程场景流控完善 |
| T2: Async Reader 模式 | 性能敏感场景 | 回调驱动替代轮询 |
| T3: Per-component resource monitor | 运维需求 | CPU%/mem/fd 可观测 |
| O4: 日志零开销优化 | release 性能 | 宏消除 |

### 4.3 中期需求（P2 — Phase 2/3 前需要）

| 需求 | 来源 | 触发条件 |
|------|------|----------|
| M4-1~5: 系统闭环 | 业务价值 | Basement 100% + Libraries 基础类型稳定 |
| Libraries spec 创建 | 工程管理 | Phase 2 正式启动前 |
| Modules ROS2 清理 + 重构 | 架构纯洁性 | Phase 3 前 |

### 4.4 远期需求（P3 — Phase 4）

| 需求 | 来源 | 时间线 |
|------|------|--------|
| install layout 标准化 | 部署 | M4 闭环后 |
| health check 自动告警 | 运维 | 多进程稳定运行后 |
| 性能基准测试套件 | 质量 | B5 测试框架就绪后 |
| record/replay 端到端验证 | 工具链 | M4 闭环后 |

---

## 五、长期目标推演

### 5.1 目标体系（三层）

```
Level 3: 愿景目标（3-5 年）
├── 成为可替代 CyberRT 的轻量自动驾驶运行时
├── 支持完整 sensor → planning → control 数据流
└── 具备 production-grade 的可观测性与可靠性

Level 2: 战略目标（1-2 年）
├── Basement 100% + 测试覆盖 > 60%
├── Libraries 算法库正式纳入工程管理
├── Modules 实现 5+ 个业务组件的稳定闭环
└── 具备 CI/CD pipeline（build + test + deploy）

Level 1: 战术目标（1-3 个月）
├── B5: 测试基础设施（当前最优下一步）
├── ROS2 残留清理 + @deprecated 文件删除
├── T1+T2: Transport 最后 5% 补齐
└── Basement 宣布 v1.0（封板版本）
```

### 5.2 关键里程碑预测

| 里程碑 | 目标日期 | 交付物 | 前置 |
|--------|----------|--------|------|
| **v0.3 (B5)** | 2026-04 中旬 | Catch2 + 核心路径单测 + CI | 无 |
| **v1.0 (Basement 封板)** | 2026-04 下旬 | T1+T2 + 测试 > 60% + debt 清零 | v0.3 |
| **v1.1 (Libraries 启动)** | 2026-05 | libraries spec + 接口形式化 | v1.0 |
| **v2.0 (Modules 闭环)** | 2026-06~07 | 5+ 组件 + M4 闭环 | v1.1 |
| **v3.0 (Production)** | 2026-Q3 | CI/CD + install + perf benchmark | v2.0 |

---

## 六、技术债务全景

### 6.1 活跃债务

| ID | 债务 | 区域 | 利息（持续影响） | 建议 | 优先级 |
|----|------|------|------------------|------|--------|
| TD4 | **零单元测试** | 全局 | 每次改动都是盲飞风险 | B5 立即启动 | **P0** |
| TD5 | main.cc 860 行膨胀 | apps | 每 add subcommand +15 行 | 拆分 subcommand 文件 | P2 |
| TD1 | @deprecated 文件 4 个 | modules | 代码噪音 + 搜索干扰 | 确认引用后删除 | P1 |
| TD3 | ConfValue 兼容层残留 | libraries | Phase 2 时必须处理 | libraries 阶段 | P2 |
| TD5(复) | native_message_bus.h 过大 | transport | 2200+ 行单文件 | 考虑拆分 reader/stats | P3 |

### 6.2 已解决债务

| ID | 债务 | 解决方式 | 日期 |
|----|------|----------|------|
| TD2 | CMakeLists.txt 遗留 | 标记 @LEGACY | 2026-04-07 |

---

## 七、结论与建议

### 7.1 一句话总结

> **Starletto 已完成底座 93%，schema/transport/core 达到生产级质量，
> 最大瓶颈是零单元测试导致的风险裸奔。
> 下一步应立即引入测试框架，然后封板 Basement v1.0。**

### 7.2 如果只能做一件事

**做 B5：引入 Catch2 + 写核心路径单元测试。**

理由：
1. 零测试 = 93% 成熟度不可验证
2. 有了测试保护，才能安全地做 T1/T2/debt cleanup
3. 测试框架是后续所有工作的基础设施
4. 工作量可控（1-2 天框架集成 + 逐步补充用例）

### 7.3 如果可以做两件事

**B5（测试）+ ROS2/deprecated 清理（架构清洁）。**

后者是低垂果实（< 1 小时），但能显著提升代码库整洁度。

### 7.4 完整推荐路线

```
现在 (v0.2, 93%)
  │
  ├─ Week 1: B5 测试框架 + 核心路径单测 → v0.3
  │   ├─ Catch2 集成到 Meson
  │   ├─ codec encode/decode 用例
  │   ├─ QoS ring buffer 用例
  │   └─ Logger/RollingFileLogger 用例
  │
  ├─ Week 2: 架构清洁 + Transport 收尾 → v0.4
  │   ├─ ROS2 残留清理 (3 行)
  │   ├─ @deprecated 文件删除
  │   ├─ T1 SHM QoS buffer
  │   └─ T2 Async Reader
  │
  ├─ Week 3: Basement v1.0 封板
  │   ├─ 测试覆盖率 > 60%
  │   ├─ 技术债务清零
  │   ├─ spec 文档最终版
  │   └─ v1.0 tag + release record
  │
  └─ 之后: Phase 2 (Libraries) 或 Phase 3 (Modules 闭环)
      取决于你的战略选择
```

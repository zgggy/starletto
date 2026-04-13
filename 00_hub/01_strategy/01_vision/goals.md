# 阶段目标与成功标准

## Phase 1：Basement 底座完善 ✅ 进行中（93%）

### 目标

让 basement 达到"可用且可观测"的水平，为业务模块提供稳定底座。

### 成功标准（完成度追踪）

| # | 标准 | 状态 | 备注 |
|---|------|------|------|
| G1 | Schema IDL + codegen + codec + CI 全闭环 | ✅ | 20 schema, codegen 7548 行 |
| G2 | Transport inproc/shm/routed + QoS + p99 统计 | ✅ | 2228 行, monitor CLI 就绪 |
| G3 | Runtime auto_configure + FrameContext + supervisor | ✅ | 2680 行, HealthChecker 已就绪 |
| G4 | Manifest Schema-typed 主通路 | ✅ | 477 行, ConfValue 死代码已清理 |
| G5 | Tools: star build/run/graph/doctor/record/replay/monitor | ✅ | 10 条 CLI 命令全部可用 |
| G6 | RollingFileLogger + 统一日志格式 + IClock 抽象 | ✅ | core 481 行, 三项全交付 |
| **G7** | **测试基础设施（单元测试覆盖 > 60%）** | ❌ | **最大缺口：零单元测试** |
| **G8** | **M4 最小闭环（sensor → planning → control）** | ⬜ | 需 libraries + modules 配合 |

### 代码资产量化

| 子模块 | 文件数 | 代码行数 | 占比 | 成熟度 | 缺口 |
|--------|--------|----------|------|--------|------|
| core | 6 | 481 | 4% | 95% | 日志零开销优化(O4) |
| schema | 13 | 7,548 | 62% | 100% | 无 |
| transport | 10 | 2,228 | 18% | 95% | SHM QoS(T1), Async Reader(T2) |
| runtime | 13 | 2,680 | 22% | 88% | DAG 调度, 测试覆盖 |
| manifest | 5 | 477 | 4% | 90% | 无重大缺口 |
| **合计** | **47** | **12,181** | **100%** | **93%** | **G7(测试) 为核心瓶颈** |

### 从 93% 到 100% 的路径

```
当前 93%
  │
  ├─→ B5: 引入测试框架 + 核心路径单测 (~+3%)  ← 最优下一步
  │     core: Logger/IClock/RollingFileLogger
  │     schema: codec encode/decode 正确性
  │     transport: QoS ring buffer / p99 计算
  │     runtime: auto_configure / FrameContext
  │
  ├─→ T1+T2: SHM QoS + Async Reader (~+2%)
  │
  └─→ 100%: Basement 底座封板，进入 Phase 2
```

## Phase 2：Libraries 算法库 ⬜ 待启动

### 前置条件

Basement ≥ 95%（即 B5 测试完成后）

### 目标

建立纯算法层，让业务模块可以复用数学/几何/IO 能力。

### 当前 assets（已有但未正式纳入工程管理）

libraries/ 目录已有实质内容：

| 子目录 | 文件数 | 估计行数 | 内容 |
|--------|--------|----------|------|
| common/math | ~8 | ~600 | bezier/geometry/interpolation/polynomial |
| common/schema | ~3 | ~200 | SchemaValue 类型系统 |
| element/type | ~15 | ~1500 | path/trajectory/shape/frenet/map/speed |
| element/container | ~5 | ~300 | decision_info 等 |
| element/decision | ~2 | ~100 | decision 定义 |
| method/algorithm | ~8 | ~800 | astar/piecewise_jerk/route_search |
| method/adaptor | ~3 | ~200 | path_generation |
| machine/* | ~10 | ~800 | FSM/machine_builder/machine_node |

**总计：~54 文件, ~4,493 行** — 已有相当规模的算法资产！

### 关键问题

- libraries 无独立 spec 文档
- libraries 内部依赖关系未梳理
- 与 basement 的边界接口未形式化定义
- 无任何测试覆盖

## Phase 3：Modules 业务闭环 ⬜ 待启动

### 前置条件

Libraries 基础类型稳定 + Basement 100%

### 目标

实现 sensor → planning → control 的完整数据流闭环。

### 当前 modules 资产

| 模块 | 文件数 | 状态 | ROS2 残留 |
|------|--------|------|-----------|
| planning/motion | ~8 | Demo 可运行 | motion.h @deprecated |
| planning/navigation | ~6 | @deprecated | refline.h 有 rclcpp |
| control/pid | ~4 | Demo 可运行 | 无 |
| setting/config | ~4 | 可用（发布 snapshot） | 无 |
| setting/parameter | ~4 | Demo 可用 | parameter.h 有 rclcpp |
| debug/debug_motion | ~3 | @deprecated | 无 |

### 关键问题

- 3 处 ROS2 残留引用（需清理）
- 3 个 @deprecated 文件需确认后删除
- 无 localization / perception 模块
- planning 只有 motion demo，缺决策/导航

## Phase 4：部署与运维 ⬜ 远期

### 目标

支持工程交付所需的安装、诊断、回放能力。

### 关键交付

- install layout 标准化
- health check 自动告警
- record/replay 端到端验证
- 性能基准测试套件

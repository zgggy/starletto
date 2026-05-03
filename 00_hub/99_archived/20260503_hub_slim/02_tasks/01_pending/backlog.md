# 全量待办 Backlog

## 待办功能需求

| ID | 标题 | 优先级 | 依赖 | 预估 | 状态 |
|----|------|--------|------|------|------|
| B5-1 | 引入 Catch2 测试框架 | P0 | 无 | — | ⬜ pending |
| B5-2 | core/schema/transport 关键路径单元测试 | P0 | B5-1 | — | ⬜ pending |
| B5-3 | CI pipeline 集成（build + test + CI guard） | P0 | B5-2 | — | ⬜ pending |
| T1 | SHM 路径 QoS buffer 扩展 | P1 | M3 | — | ⬜ pending |
| T2 | Async Reader 模式（回调驱动） | P1 | M3 | — | ⬜ pending |
| T3 | Per-component resource monitor（CPU%/mem/fd） | P1 | F1 | — | ⬜ pending |
| M4-1 | localization 模块雏形 | P2 | basement stable | — | ⬜ pending |
| M4-2 | perception 模块（可 stub） | P2 | M4-1 | — | ⬜ pending |
| M4-3 | planning 决策/导航完善 | P2 | M4-2 | — | ⬜ pending |
| M4-4 | control PID 完善 | P2 | M4-3 | — | ⬜ pending |
| M4-5 | 多进程闭环端到端验证 | P2 | 全部 | — | ⬜ pending |

## 统计

- 待办功能：10
- 进行中：0
- 已归档：29（M0-M3 基础 17 + P0-P5 增强 6 + B1-B2 工程化 2 + F1-F6 观测性 6）

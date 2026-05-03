# 版本定义与语义

## 版本格式

`v{MAJOR}.{MINOR}` — 简化语义版本，不使用 patch。

## 当前版本

| 版本 | 日期 | 阶段 | 关键交付 |
|------|------|------|----------|
| v0.1 | 2026-04-07 | M0-M3 基础底座 | Schema/Transport/Runtime/Manifest/Tools 主通路 |
| v0.2 | 2026-04-11 | 工程化+观测性补全 | B1-B2 + F1-F6（RollingFileLogger/monitor/p99/health） |

## 语义规则

| 变更类型 | 版本行为 |
|----------|----------|
| 新功能（向后兼容） | MINOR + 1 |
| 破坏性 API 变更 | MAJOR + 1 |
| Bug fix / 内部重构 | 不变号，记录在 changelog |
| Spec 文档更新 | 不变号，记录在 changelog |

## 版本里程碑映射

| 里程碑 | 目标版本 | 状态 |
|--------|----------|------|
| M0 Native-only 雏形 | v0.1 | ✅ 完成 |
| M1 Schema v1 | v0.1 | ✅ 完成 |
| M2 Transport v1 | v0.1 | ✅ 完成 |
| M3 Runtime v1 | v0.1 | ✅ 完成 |
| M4 系统闭环 | v0.3 | ⬜ 待启动 |
| B5 测试基础设施 | v0.2 | ⬜ 待启动 |

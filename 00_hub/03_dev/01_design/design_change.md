# 设计变更记录

| 日期 | 变更内容 | 影响范围 | 决策依据 |
|------|----------|----------|----------|
| 2026-04-07 | 三工程架构边界确定 | 全局 | workplan §0 |
| 2026-04-07 | ConfValue 退出运行时主通路 | manifest/schema | basement_design §1.2 |
| 2026-04-07 | routed 去 env 注入，改 runner→worker 参数传递 | transport/runtime | transport §11 |
| 2026-04-11 | F1-F6 观测性功能新增 | transport/core/runtime | milestone M3 扩展 |

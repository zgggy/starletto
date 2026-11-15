# modules/planning —— 规划

当前只有 `motion` 示例组件（`planning.motion`）。它的主 trigger 由三路假上游
节点驱动：`/perception/prediction`、`/localization/pose`、`/control/chassis`；
两路 setting snapshot 只作为参数/配置输入，不参与 trigger。

- 进程组：`planning`（multi_thread，4 线程）
- 输出：`/planning/trajectory`
- 假上游：`modules/perception/prediction`、`modules/localization/pose`、
  `modules/control/chassis`
- 辅助资产：`motion/frame/`、`motion/machine/` 是未接线的规划数据/状态机骨架，
  按用户资产保留，不当作死代码删除。

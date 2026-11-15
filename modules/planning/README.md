# modules/planning —— 规划

当前只有 `motion` 示例组件（`planning.motion`）。上游的 perception/prediction、
localization/pose 还没有实现，因此 `motion` 目前只订阅 setting 的两路 snapshot
来维持可运行闭环；真实规划链路的主输入见 [motion/README.md](motion/README.md)。

- 进程组：`planning`（multi_thread，4 线程）
- 输出：`/planning/trajectory`
- 辅助资产：`motion/frame/`、`motion/machine/` 是未接线的规划数据/状态机骨架，
  按用户资产保留，不当作死代码删除。

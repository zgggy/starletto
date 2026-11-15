# modules/control —— 控制

当前只有 `pid` 组件（`control.pid`）：

- 读 `/planning/trajectory`（轨迹）；
- 写 `control.command`（`ControlCommand`）。

进程组 `control`。`control/chassis` 等底盘接口尚未实现；实现后按生产者在前
的通道命名（`/control/chassis`）发布底盘状态，供 planning 消费。

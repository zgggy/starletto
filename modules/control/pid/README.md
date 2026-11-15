# control.pid

最小 PID 控制闭环：

| 方向 | 端口 | 通道 | 类型 |
|---|---|---|---|
| 读 | `planning_trajectory` | `/planning/trajectory` | `TrajectoryOutput` |
| 写 | `command` | `/control/command` | `ControlCommand` |

`component.conf` 的 `trigger.readers` 让它在轨迹到达时运行；`process_once()`
用 `read_latest` 取最新轨迹，计算后 `write("command", cmd)`。e2e 延迟会写日志。

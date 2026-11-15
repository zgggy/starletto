# planning.motion

规划示例组件，用来验证 runtime/transport 的装配、调度与录制闭环。

## 当前已接线

| 方向 | 端口 | 通道 | 类型 |
|---|---|---|---|
| 读 | `parameter_snapshot` | `/setting/parameter/snapshot` | `ParameterSnapshot` |
| 读 | `config_snapshot` | `/setting/config/snapshot` | `ConfigSnapshot` |
| 写 | `trajectory` | `/planning/trajectory` | `TrajectoryOutput` |

Trigger：`readers [parameter_snapshot, config_snapshot]` + `cycle_ms=100`，
两路快照到齐后按 sync 语义运行。

## 目标主链路（待上游模块接入）

按最终数据流，`planning.motion` 的主 trigger 应该来自：

| 来源模块 | 逻辑组件 ID | 输入通道 |
|---|---|---|
| perception/prediction | `perception.prediction` | `/perception/prediction` |
| localization/pose | `localization.pose` | `/localization/pose` |
| control/chassis | `control.chassis` | `/control/chassis` |

这些模块/通道尚未实现，所以现在不能写进 `trigger.readers`——否则
SyncTrigger 会永远等不到帧，planning 无法启动。等上游落地后：

1. 在 `component.conf` 的 `readers` 增加三个端口（名字与通道一致）；
2. 在 `motion_component.h` 的 `process_once()` 用
   `read_latest<T>("perception_prediction")` 等读取；
3. 把 `trigger.readers` 扩为四路（含两路 setting snapshot），确认 sync 语义。

## 资产

`frame/`、`machine/` 是规划数据模型与状态机骨架，当前未接线但按设计保留。

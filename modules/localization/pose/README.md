# localization.pose（stub）

上游假节点：按 50ms 周期发布合成定位 `/localization/pose`。
当前按半径 10m、速度 5m/s 的圆周运动生成位姿，用于驱动 planning 的
SyncTrigger 和 `MotionInput`；不包含定位算法。

- 组件类：`star::localization::PoseComponent`
- 输出：`Pose{timestamp_ns, x, y, z, heading, linear_velocity, angular_velocity}`
- schema：`modules/setting/schema/localization/pose.schema`

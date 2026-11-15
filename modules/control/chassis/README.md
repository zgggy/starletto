# control.chassis（stub）

上游假节点：按 50ms 周期发布合成底盘状态 `/control/chassis`。
速度/转角做小幅正弦变化，供 planning 读取车辆状态；不包含底盘驱动。

- 组件类：`star::control::ChassisComponent`
- 输出：`Chassis{timestamp_ns, speed, steering_angle, throttle, brake, gear}`
- schema：`modules/setting/schema/control/chassis.schema`

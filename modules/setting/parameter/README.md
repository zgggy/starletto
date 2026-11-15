# setting.parameter

- 输出：`/setting/parameter/snapshot`（`ParameterSnapshot`，周期 1000ms）；
- 服务：`/setting/parameter/get`、`/setting/parameter/set`、`/setting/parameter/list`，
  通道 `<service>/request` / `<service>/response`；
- 输入：`parameter/config` 初始参数；
- 进程组：`parameter`。

服务 handler 在 `parameter_component.h` 中实现，响应体携带 ok/message；
`parameter_service_demo/` 是独立客户端示例。

# modules/setting —— 配置与参数

两个组件共享同一套 schema（`modules/setting/schema/`）：

- `setting.config`：读 `files/` 配置，周期发布 `setting.config.snapshot`；
- `setting.parameter`：维护参数存储，发布 `/setting/parameter/snapshot`，
  并提供 get/set/list 三个 typed service。

服务通道按普通通道派生：`<service>/request` / `<service>/response`
（例如 `/setting/parameter/get/request`）。schema 由任一 setting 插件在注册时
幂等注册，app 不需要知道 setting 的存在。

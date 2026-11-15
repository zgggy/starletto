# setting.config

- 输入：`files/` 下的配置文件（schema typed，启动时校验）；
- 输出：`setting.config.snapshot`（`ConfigSnapshot`，周期 1000ms）；
- 进程组：`setting`。

`init()` 只做配置加载；端口由 `auto_configure()` 按 `component.conf` 建好。

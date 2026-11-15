# apps —— 可执行入口

| 目标 | 用途 |
|---|---|
| `native_runner/native_runner` | 动态装配宿主：按 `component.conf` dlopen 插件（调试） |
| `native_runner/native_runner_static` | 静态装配宿主：内置模块已编入，`static_link=true` 不 dlopen（部署） |
| `record_replay_tool/record_replay_tool` | 录制信息/回放/写回通道（`star replay`） |
| `parameter_service_demo` | 参数服务客户端示例 |

`native_runner` 支持单进程与 `--multi-process` supervisor（ready 握手、重启、
录制现场），不直接 include 业务模块；模块通过 `star_register_components`
自注册。app-boundary 门禁会检查这一点。

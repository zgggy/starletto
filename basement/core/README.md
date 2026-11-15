# basement/core —— 基础设施

`core` 是最底层：只依赖 C++ 标准库和系统调用，不依赖 schema/transport/runtime。

## 内容

| 路径 | 作用 |
|---|---|
| `logger/` | 同步/异步/滚动文件日志、结构化字段、per-module 级别 |
| `exception.h` | `RuntimeError` 等运行时异常基类 |
| `common_types.h` | `TypedConstPtr`/`TypedPtr`（运行时类型擦除 + schema 身份） |
| `type_id.h` | `SchemaTypeId{name, version, hash}` |
| `build_info.h` | 编译/工具链指纹；插件的 `STAR_EXPORT_TOOLCHAIN_FINGERPRINT()` |
| `fd_guard.h` | RAII fd 管理 |
| `stable_hash.h` | FNV-1a，用于跨工具链稳定的 shm 段名 |

## 约束

- 不得 include `basement/schema`、`transport`、`runtime` 或业务模块。
- 日志不得在热路径做格式化拼接；需要时用 `AsyncLogger`。
- `TypedConstPtr` 的身份以 `schema_type_id` 为准，`typeid` 只用于调试断言。

测试：`core_logger`（6 用例）以及各层对 logger 的间接覆盖。

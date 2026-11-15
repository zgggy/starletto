# tests —— 测试

测试框架是 `support/star_test.h`（Catch2 风格子集），每个源文件与
`support/test_main.cc` 组成一个 Meson test 目标。

| 目录 | 内容 |
|---|---|
| `schema/` | codec/registry/fixed layout/随机输入/import/多 root |
| `transport/` | native/shm/routed/record/replay/rpc/并发/stress |
| `runtime/` | component/plan/scheduler/trigger/process/观测/插件 ABI |
| `core/` | logger |
| `support/` | 测试框架与 main |

约定：

- 随机输入使用固定种子，`STAR_FUZZ_ITERS` 可调；
- UBSan 全量必过；ASan 在 Linux CI 跑；
- 插件生命周期测试注意 `ComponentRegistry` 必须先于 `DlHandle` 析构；
- 新增测试直接加进 `tests/meson.build`，`star test` 会运行；
- 产品 `component.conf` 的通道地址必须是 `/...`（由 plan 校验）；低层 transport
  单元测试可以使用任意标签名来隔离测试数据。

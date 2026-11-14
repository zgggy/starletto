# 测试与 sanitizer

## 1. 常规

```bash
star build            # 编译
star test             # meson test
star check            # 14 项门禁（含 selftest / doctor --deep / runtime-smoke）
```

## 2. 随机输入（确定性 fuzz-like）

`tests/schema/robustness_test.cc` 与 `tests/runtime/frame_robustness_test.cc` 使用
固定种子的伪随机输入覆盖：

- binary codec（三种 decode_policy 组合，5000 次采样后收敛为 1500×3）
- schema 值解析器 / 原生 schema loader 的任意文本
- 固定布局帧的字节变异与完全随机帧

不依赖 libFuzzer，离线可跑。采样次数可用环境变量调整：

```bash
STAR_FUZZ_ITERS=100 star test
```

## 3. UBSan（已验证）

```bash
meson setup build-ubsan -Db_sanitize=undefined -Db_lundef=false
meson compile -C build-ubsan
meson test -C build-ubsan
```

2026-11-14 用这套配置跑全量 32 套用例，发现并修复两处真实 UB：

1. `FixedLayoutView` 的槽位起点在 packed 26 字节帧头之后，不满足
   `FixedFieldSlot` 的 8 字节对齐；直接 `reinterpret_cast` 访问是 UB。
   改为 `memcpy` 读槽（保持 v2 帧格式不变）。
2. `BinaryCodec` 的 best-effort 路径把 NaN/Inf/越界 double 强转 int64。
   现在只接受有限且在 `[INT64_MIN, 2^63)` 内的值，否则按转换失败报错。

## 4. ASan 的现状

在 macOS 27 + Apple clang 26 上，ASan 版本的测试二进制会在 dyld 初始化阶段
死锁（栈在 `__asan::AsanInitInternal → StaticSpinMutex::LockSlow`），与项目代码无关。
因此：

- 本机用 UBSan 验证；
- ASan 放到 Linux CI（见 `.github/workflows/ci.yml`）或可用的 clang 环境。

## 5. CI 模板

`.github/workflows/ci.yml` 在 Ubuntu 上跑：

1. `star build` + `star test` + `star check --skip-build`
2. UBSan 构建 + 全量测试

GitHub Actions 尚未实际启用；模板用于把本地门禁搬到 PR。

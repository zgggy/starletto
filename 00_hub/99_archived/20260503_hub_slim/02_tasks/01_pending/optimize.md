# 待办优化项

## 已识别优化方向

| ID | 标题 | 区域 | 说明 | 状态 |
|----|------|------|------|------|
| O1 | native_message_bus.h 拆分 | Transport | 单文件过大，考虑拆分 reader/writer/stats | ⬜ |
| O2 | schema_codegen.py 性能 | Schema | 大量 schema 时 codegen 耗时 | ⬜ |
| O3 | meson build 缓存优化 | Tools | 增量编译效率 | ⬜ |
| O4 | 日志宏零开销 | Core | release 模式下 logD 完全消除 | ⬜ |

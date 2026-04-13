# 性能测试

## 当前状态

⚠️ **暂无正式性能基准测试**。以下为手动观察值。

## 手动观察数据（2026-04-11）

| 指标 | 值 | 场景 |
|------|-----|------|
| 单帧处理延迟 | ~100 us | motion component, inproc |
| p99 延迟 | ~112 us | 3 frames, STAR_TRANSPORT_STATS=1 |
| p999 延迟 | ~112 us | 同上 |
| publish 频率 | ~10 Hz | --sleep-ms=10, 3 frames |
| 构建时间 | ~5 s | 增量编译（10 targets） |
| codegen 时间 | < 1 s | 20 schemas |
| CI Guard 时间 | < 1 s | 20 files diff |

## B5 性能测试计划

待建立：
- 吞吐量 benchmark（publish/s 往不同 QoS 配置下）
- 延迟分布 benchmark（不同 payload size）
- 内存占用 baseline
- SHM vs inproc 对比

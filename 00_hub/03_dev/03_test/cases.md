# 测试用例

## 当前状态

⚠️ **暂无正式单元测试框架**。当前测试方式为端到端手动验证。

## 端到端验证矩阵（已通过）

| # | 命令 | 验证项 | 状态 |
|---|------|--------|------|
| 1 | `./star build` | 全量编译 10/10 零错误 | ✅ |
| 2 | `./star run --frames=3 motion.conf` | 单进程运行 + 组件加载 + 消息发布 | ✅ |
| 3 | `STAR_TRANSPORT_STATS=1 ./star run ...` | Transport 统计输出（含 p99/p999） | ✅ |
| 4 | `STAR_LOG_FILE=/tmp/test/log ./star run ...` | RollingFileLogger 落盘 | ✅ |
| 5 | `./star monitor --transport=inproc motion.conf` | Monitor CLI 输出 | ✅ |
| 6 | `bash tools/check_schema_generated.sh` | CI Guard 20/20 .s.h 通过 | ✅ |
| 7 | `./star list planning/module.conf` | 列表命令 | ✅ |
| 8 | `./star graph=component-dot planning/module.conf` | 图形输出 | ✅ |
| 9 | `./star doctor [--deep] planning/module.conf` | 诊断（含 dlopen 检查） | ✅ |
| 10 | `./star run --multi-process planning/module.conf` | 多进程编排 | ✅ |

## B5 计划引入的单元测试

> 待 Catch2/Googletest 引入后补充。

### 优先覆盖路径

1. **Schema**：IDL 解析 / codec 编解码 / compat 检测
2. **Core**：Logger 格式化 / IClock 各实现 / RollingFileLogger roll 逻辑
3. **Transport**：QoS ring buffer / compute_percentile / CorrelationId 生成
4. **Runtime**：auto_configure / FrameContext 注入 / Manifest 解析

# 技术债务清单

## 已识别技术债务

| ID | 标题 | 区域 | 影响 | 建议 | 状态 |
|----|------|------|------|------|------|
| TD1 | @deprecated 文件 + ROS2 残留 | modules/messages | 代码噪音+架构不纯 | 删除 4 文件 + messages/ + package.xml | ✅ **done (v0.2.1)** |
| TD2 | CMakeLists.txt 遗留 | 全局 | 与 Meson 混淆 | 全部标记 @LEGACY | ✅ **done (v0.2.1)** |
| TD3 | ConfValue 兼容层残留 | libraries/common | libraries 正式开发时清理 | libraries 阶段处理 | ⬜ **重评估：schema_value.h 是活跃资产(35文件引用)** |
| TD4 | 无单元测试覆盖 | 全局 | 回归风险 | B5 测试基础设施（放缓） | ⬜ → **CI Guard 已实现 (star check)** |
| TD5 | main.cc 过大（860 行） | apps | 可维护性 | 拆分 subcommand 为独立文件 | ✅ **done (v0.2.2)** |

## 债务趋势

```
总债务：5 项
已解决：3 项（TD1 清理 + TD2 @LEGACY + TD5 拆分）
待处理：2 项
新增：0 项
```

## v0.2.1 清理详情（TD1）

### 删除的文件

| 类别 | 数量 | 行数/文件数 |
|------|------|-------------|
| @deprecated .h | 4 个 | 799 行 |
| messages/ 目录 | ~40 个 | 纯 ROS2 msg 定义 |
| source tree package.xml | 4 个 | ROS2 包清单 |
| 配套 CMakeLists.txt | 4 个 | 与 deprecated .h 对应 |

### 标记 @LEGACY 的文件

| 文件 | 原状态 |
|------|--------|
| basement/CMakeLists.txt | 已是 @LEGACY |
| modules/planning/CMakeLists.txt | → @LEGACY |
| modules/setting/CMakeLists.txt | → @LEGACY |
| modules/debug/CMakeLists.txt | → @LEGACY |
| modules/setting/config/CMakeLists.txt | → @LEGACY |

### 清理效果

- ROS2 代码引用：**~50 行 → 0 行**
- @deprecated .h 文件：**4 个 → 0 个**
- source tree package.xml：**4 个 → 0 个**（artifacts/ 中残留为 install 产物，下次 install 时清除）

## v0.2.2 收尾详情（TD5）

### main.cc 拆分结果

| 文件 | 行数 | 职责 |
|------|------|------|
| `types.h` | 35 | RunOptions / Inputs / g_argv0 声明 |
| `main.h` | 165 | main() 入口 + parse_args + crash_handler |
| `app.h` | 508 | App 类全部 subcommand 实现 |
| `runner_utils.h` | 85 | manifest/path/library 解析工具函数 |
| `main.cc` | 1 | 单行 include |
| **合计** | **794** | **原 860 行，-8%** |

## T1 Transport 收尾详情

### ShmMessageBus 新增能力

| 能力 | NativeMessageBus (inproc) | ShmMessageBus (shm) | 状态 |
|------|---------------------------|---------------------|------|
| Per-reader QoS ring buffer | ✅ | ✅ **新增** | **对齐** |
| DropPolicy (KEEP_LAST/DROP_NEWEST) | ✅ | ✅ **新增** | **对齐** |
| Latency p99/p999 | ✅ | ✅ **新增** | **对齐** |
| max_queue_depth 统计 | ✅ | ✅ **新增** | **对齐** |

**效果：Transport 层 inproc / shm 双路径能力完全对齐 → 100%**

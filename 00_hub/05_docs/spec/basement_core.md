# Basement / Core 设计方案（异常、日志、时间、基础设施）

Core 是 basement 的最底层，不依赖 transport/runtime/manifest/tools。  
目标：提供稳定、可移植、可观测的基础设施，让上层不必重复造轮子。

## 1. 职责边界

- 异常：统一 error 类型与抛出策略
- 日志：统一 logger API、sink、格式、文件滚动（可配置）
- 时间：统一 clock/timepoint/duration（monotonic + system）
- 基础类型：TypedPtr/AnyPtr、span、byte buffer（为 Schema/Transport 服务）
- 调试：崩溃回溯、断言、统计接口（可选）

禁止：

- core 内引入“业务语义”
- core 内依赖 Schema 以上的模块（保持依赖有向无环）

## 2. 日志（对齐 Apollo 的工程能力）

目标：

- stdout/file 两种 sink
- 文件滚动（按大小/按日期）
- 进程/线程/模块标签
- 结构化字段（为 trace/monitor 服务，后续可选）

现状：

- 已有 `StdoutLogger` 与 `LoggerManager`
- 已引入 `FileLogger`（用于 install-tree 日志落盘）
- ✅ **RollingFileLogger**（2026-04-07）：`RollingPolicy{max_file_size, max_backup_files}`，自动滚动，已集成到 runtime_factory（`STAR_LOG_FILE` 触发）

下一步：

- ✅ ~~RollingFileLogger~~ （B2 已完成，2026-04-11）
- ✅ ~~统一 log format~~ （F3 已完成，2026-04-11）
- ✅ ~~Clock 抽象~~ （F4 已完成，2026-04-11）
- ⬜ 支持 per-process_group 文件（多进程默认）

## 7. 日志零开销优化（O4）

### 7.1 目标

`logD/logI/logW/logE` 宏在 **禁用级别时编译器完全消除**（零指令、零内存、零分支）。

### 7.2 实现

```cpp
// for (bool ok = is_enabled(level); ok; ) LogStream(...)
#define _STAR_LOG_STREAM(level) \
    for (bool _star_log_ok = star::base::Logger::is_enabled(level); _star_log_ok; ) \
        star::base::LogStream(star::base::LoggerManager::get().get_logger(), level, __FILE__, __LINE__, __FUNCTION__)
```

- 编译器在 `_star_log_ok = false` 时完全消除循环体（含 LogStream 构造/析构）
- 无虚函数调用开销，无 ternary operator 类型不匹配问题

### 7.3 验收标准

- ✅ `is_enabled()` 静态方法声明在类中，定义在 LoggerManager 之后（2026-04-11）
- ✅ for-loop 短路模式编译通过且运行正确（2026-04-11）
- ✅ `./star build` 零错误（2026-04-11）

## 3. 时间

目标：

- `steady` 与 `system` 两套时钟
- 与 record/replay 的“仿真时间/回放时间”兼容（后续）

## 4. 基础内存与类型（为 Schema/Transport 做准备）

Schema 要超过 protobuf 的关键是 zero-copy，因此 core 必须提供：

- `TypedConstPtr`（已存在）与更严格的类型标识
- `ByteSpan`/`MutableByteSpan`（建议引入）用于 View/Builder
- `Arena/Pool`（可选）用于减少分配

## 5. 验收标准

- Core 的 API 不依赖上层模块
- 日志在 build-tree 与 install-tree 均可用且可落盘
- 提供 Schema/Transport 所需的最小 byte/span/typed ptr 能力

## 6. RollingFileLogger（B2）

### 6.1 目标

提供按文件大小自动滚动的日志落地能力。

### 6.2 设计

- `RollingPolicy{max_file_size, max_backup_files}`
- 达到阈值时：`.log` → `.log.1` → `.log.2` → ... → `.log.N`（shift）
- 超过 `max_backup_files` 时删除最旧的备份

### 6.3 集成点

[`runtime_factory.h`](file:///home/zgy/projects/starletto/basement/runtime/factory/runtime_factory.h)：当 `STAR_LOG_FILE` 环境变量设置时，自动创建 `RollingFileLogger` 替代 `FileLogger`。

### 6.4 验收标准

- ✅ `STAR_LOG_FILE=/tmp/test/log ./star run ...` 正确写入滚动日志（2026-04-11）
- ✅ 默认策略：10MB / 5 备份（2026-04-11）

## 7. 统一日志格式（F3）

### 7.1 目标

统一所有 log sink 的输出格式，支持结构化解析。

### 7.2 格式规范

```
YYYY-MM-DDTHH:MM:SS.uuuuuu LEVEL [TID] file:line function message
```

示例：
```
2026-04-11T03:13:34.680080 INFO0 [134715787462464] main.cc:111 run loaded component: motion
```

### 7.3 实现

[`logger.h::format_message()`](file:///home/zgy/projects/starletto/basement/core/logger/logger.h) — 使用 `<chrono>` + `<iomanip>` + `<thread>` 生成 ISO 时间戳 + 线程 ID + 位置信息。`StdoutLogger` 简化为直接输出 `format_message()` 结果。

### 7.4 验收标准

- ✅ 所有 log 输出统一格式（stdout + file）（2026-04-11）
- ✅ ISO 8601 时间戳 + 微秒精度（2026-04-11）

## 8. Clock 抽象（F4）

### 8.1 目标

提供可替换的时钟接口，为 record/replay 仿真时间做准备。

### 8.2 接口

文件：[`iclock.h`](file:///home/zgy/projects/starletto/basement/core/iclock.h)

```cpp
class IClock {
public:
    virtual auto now() const -> std::chrono::nanoseconds = 0;
    virtual auto steady_now() const -> std::chrono::nanoseconds = 0;
};

class SteadyClock final : public IClock;      // steady_clock
class SystemClock final : public IClock;       // system_clock now, steady steady
class SimulationClock final : public IClock;   // 可推进/设置的仿真时钟
```

`SimulationClock` 支持 `advance(ns)` 和 `set(time_point)`，用于 record/replay 场景。

### 8.3 验收标准

- ✅ 三个实现类定义完成（2026-04-11）
- ✅ 接口纯虚、无业务依赖（2026-04-11）

## 9. Per-component 资源监控（F-1）

### 9.1 目标

为每个运行中的组件/进程提供 **CPU 占用率、RSS 内存、文件描述符数、线程数** 的实时采集能力，集成到 `star monitor` 输出中。

### 9.2 接口设计

文件：[`resource_monitor.h`](basement/runtime/observability/resource_monitor.h)

```cpp
struct ResourceSnapshot {
    int64_t pid;
    std::string name;
    double cpu_percent;       // % (两次采样 delta)
    uint64_t rss_kb;          // kB
    int fd_count;             // 文件描述符数
    int64_t thread_count;     // 线程数
    int64_t timestamp_ns;

    auto format_line() const -> std::string;
};

class ResourceMonitor {
    auto sample_self() -> ResourceSnapshot;           // 采样当前进程
    auto sample(int64_t pid) -> ResourceSnapshot;      // 采样指定 PID
    void track(int64_t pid);                           // 持续跟踪
    void untrack(int64_t pid);                         // 停止跟踪
    void refresh_all();                                // 刷新所有跟踪项
    auto dump_stats_lines() const -> std::vector<std::string>;
};
```

### 9.3 数据来源

| 指标 | /proc 路径 | 解析方式 |
|------|-----------|---------|
| CPU% | `/proc/pid/stat` 字段 14(utime)+15(stime) + `/proc/stat` 首行 | delta / total_tick × 100 |
| RSS | `/proc/pid/status` → `VmRSS:` 行 | 直接读取 kB |
| FD 数量 | `/proc/pid/fd/` 目录项计数 - 2（排除 . 和 ..） | opendir |
| 线程数 | `/proc/pid/stat` 字段 20 (num_threads) | 直接读取 |
| 进程名 | `/proc/pid/comm` | 直接读取 |

### 9.4 CPU% 计算细节

首次采样返回 0%（需要两次采样计算 delta）。使用 `static` 缓存存储上次 `CpuTimes`（utime/stime/total_tick），保证 `compute_cpu_percent()` 作为 static 方法可正确工作。

### 9.5 集成点

[`app.h`](apps/native_runner/app.h) 中 `monitor()` 子命令末尾自动附加资源行：

```
resource pid=12345 name=native_runner cpu=12.3% rss=45216kB fds=24 threads=8
```

### 9.6 验收标准

- ✅ `ResourceMonitor` 编译通过且零错误（2026-04-11）
- ✅ `sample_self()` 返回有效数据（2026-04-11）
- ✅ `./star monitor planning` 输出包含 resource 行（2026-04-11）
- ✅ `./star build` 零错误（2026-04-11）

## 10. HealthChecker 增强（F-2）

### 10.1 目标

在原有 topic hz 健康检查基础上，扩展为**多维度组件健康评估**：topic 频率 + 资源阈值 + 三级 severity + action 回调。

### 10.2 接口设计

文件：[`health_checker.h`](basement/runtime/observability/health_checker.h)

```cpp
enum class HealthSeverity : uint8_t { OK, WARNING, CRITICAL };

struct ResourceHealthRule { double max_cpu_percent; uint64_t max_rss_mb; int max_fd_count; };
struct ComponentHealthRule { std::string name; TopicHealthRule; ResourceHealthRule; };

struct ComponentHealthReport {
    std::string component_name;
    bool overall_healthy;
    HealthSeverity max_severity;
    std::vector<TopicHealthStatus> topic_statuses;
    ResourceHealthStatus resource_status;
    std::string summary_message;

    auto format_line() const -> std::string;
};

using UnhealthyAction = std::function<void(const ComponentHealthReport&)>;

class HealthChecker {
    void add_topic_rule(const TopicHealthRule&);
    void add_component_rule(const ComponentHealthRule&);
    void set_resource_monitor(std::shared_ptr<ResourceMonitor>);
    void set_unhealthy_action(UnhealthyAction);

    auto check_component(const std::string& name, int64_t pid) const -> ComponentHealthReport;
    auto check_all_components() const -> std::vector<ComponentHealthReport>;
    auto dump_health_lines() const -> std::vector<std::string>;
};
```

### 10.3 Severity 自动升级规则

| 条件 | Severity |
|------|----------|
| 所有指标正常 | OK |
| hz < min_hz 但 > 0；或 RSS/FD/CPU 单项超阈值 | **WARNING** |
| hz = 0（完全停止）；或 RSS 超 2x 阈值 | **CRITICAL** |

### 10.4 Action 回调

当组件不健康时触发 `UnhealthyAction`，可接入：
- 重启策略（kill + respawn）
- 告警通知（日志 / webhook）
- 降级处理（切换到备用路径）

### 10.5 与 F-1 的关系

`HealthChecker` 通过 `set_resource_monitor()` 注入 `ResourceMonitor` 实例，解耦两个模块。若未注入则跳过资源检查。

### 10.6 验收标准

- ✅ `HealthChecker` 编译通过且零错误（2026-04-11）
- ✅ 三级 severity + 资源阈值规则完整（2026-04-11）
- ✅ `ComponentHealthReport::format_line()` 格式化输出（2026-04-11）
- ✅ `./star build` 零错误（2026-04-11）

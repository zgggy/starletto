# Basement / Runtime / Process 设计方案（多进程编排与运维）

Process 子模块的职责：把“组件装配”提升为“可部署进程拓扑”，并提供监督、日志、健康检查与退出策略。

## 1. 目标（对齐 Apollo mainboard + launch 体验）

对齐 Apollo 的工程体验：

- 一个入口启动多个 component（按 dag/launch 描述拓扑）
- 支持按 group 隔离资源（CPU/线程/优先级/NUMA）
- 崩溃可定位、可回收、可重启
- 工具链可观察（运行状态、频率、延迟、丢帧）

## 2. 术语

- **process_group**：部署与资源隔离单位（同组同进程为默认）
- **worker**：承载 component 的进程实例
- **supervisor**：负责拉起/监控 worker 的父进程（可以是 runner 自身或独立守护）

## 3. 现状与差距

现状：

- `star run --multi-process` 已能按 process_group fork/exec worker
- 日志可按组拆分到不同文件
- 跨组通信已可走 shm（零拷贝）

差距（必须补齐）：

- 缺统一的 ProcessPlan（由 manifest 生成）
- 缺监督策略：退出码、重启次数、退避、联动 stop
- 缺健康检查：心跳、topic 频率、延迟阈值、资源阈值
- 缺系统化的运行状态输出（供 monitor/doctor 使用）

## 4. ProcessPlan（由 Manifest 生成）

必须生成：

- `ProcessGroup` 列表：group 名称、资源配置、组件列表
- 每个 worker 的启动参数：transport 模式、日志路径、模块根目录

本轮最小落地约束：

- 至少先落地生成态 `ProcessPlan`
- `ProcessPlan` 的最小 group 字段：
  - `name`
  - `component_names`
  - `component_conf_paths`
  - `produced_topics`
  - `consumed_topics`
- `star run --multi-process` 主路径必须优先从 typed `ProcessPlan` 分组与生成 route plan
- 依赖关系：先启动 setting 再启动 planning 等（可选）

## 5. 监督与退出策略

### 5.1 基本策略（P1）

- worker 非 0 退出：标记失败并聚合到总体退出码
- 支持 `--restart`（后续）：按策略重启 worker（次数/退避）

### 5.2 健康检查（P2）

- liveness：进程存活 + 心跳 channel
- readiness：关键 channel 的 writer/reader 就绪
- performance：延迟/丢帧/CPU 使用超阈值告警

## 6. 日志策略（必须工程化）

- 默认 install-tree 下日志落到 `artifacts/logs/`
- 多进程默认按 group 拆分：`<group>_<timestamp>.log`
- 支持结构化日志字段（后续用于 trace/monitor）

## 7. 验收标准

P1：

- 多进程样板链路可跑通（setting+planning）
- 任一 worker 崩溃可定位（日志+回溯），整体退出码正确

P2：

- supervisor 支持重启策略
- 支持健康检查与监控输出

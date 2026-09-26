# Basement / Runtime / Trigger 设计方案（数据驱动触发与帧语义入口）

Trigger 子模块的职责：把“时间/数据到达”转成可调度的任务触发，并为帧一致性提供统一入口。

## 1. 目标（对齐 Apollo 数据驱动思想）

- 支持 timer trigger（周期）
- 支持 topic trigger（单 topic 到达）
- 支持 multi-topic trigger（多输入同步/等待，帧一致性基础）
- 触发必须产出明确的 `frame_id` 或 `stamp`（后续 DAG/FrameContext 使用）

## 2. 触发器分类

- TimerTrigger：周期性触发
- TopicTrigger：某 channel 新消息触发
- SyncTrigger：多个 channel 同步触发（按时间戳/帧 id 对齐）
- ManualTrigger：用于测试/调试/回放

## 3. 帧语义（FrameContext）

帧语义是自动驾驶系统“确定性”的关键：

- 以某个主消息的时间戳作为帧标识（或显式 frame_id）
- 同一帧内：各组件按 DAG/依赖有序推进
- 不同帧：允许流水线并行，但需可控（最大 in-flight 帧数）

Trigger 负责：

- 从输入消息提取 stamp/frame_id（schema 类型应提供统一字段或 meta）
- 在缺省情况下生成 frame_id（timer 触发）

## 4. 与 Schema/Transport 的关系

- Trigger 只能通过 reader 获取消息 view（inproc ptr 或 shm bytes view）
- Trigger 不解析业务字段；只读取 schema meta（timestamp/frame_id）或由 manifest 指定提取规则

## 5. 验收标准

P1：

- timer/topic trigger 稳定可用并可观测

P2：

- sync trigger 支持多 topic 同步（用于 planning 主链路帧一致性）
- FrameContext 与 scheduler/DAG 联动跑通

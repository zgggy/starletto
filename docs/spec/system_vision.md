# 自动驾驶系统完整形态推演（对齐 CyberRT / Autoware / ROS2 完全体）

本文档回答两个问题：

1) 一个“完整自动驾驶系统”的工程形态应该是什么样（对齐参考体系）  
2) Starletto 应该如何拆分工作，把系统落到可迭代的模块与里程碑上  

## 1. 参考体系抽象（取长补短）

### 1.1 Apollo CyberRT（工程化强）

关键能力抽象：

- 强类型消息体系（proto + descriptor + 兼容演进）
- 零拷贝通信（shm / loan）
- 调度与执行模型（任务、优先级、资源控制、观测）
- record/replay 与工具链闭环（channel/monitor/recorder）

### 1.2 Autoware（组件化强）

关键能力抽象：

- 功能模块清晰（perception/localization/planning/control/…）
- 面向部署的参数/配置体系（大量参数化、可复用栈）
- 与 ROS2 生态整合（消息、工具、可视化、调试）

### 1.3 ROS2 完全体（生态与通用性强）

关键能力抽象：

- 多语言、工具链、可视化、生态适配
- 通信 QoS、发现机制、分布式
- 复杂系统的可组合性与运维经验

## 2. Starletto 的“完整系统形态”定义

Starletto 的目标不是复制 ROS2 全生态，而是把“自动驾驶工程交付”所需的底座能力做到对齐并在关键点超过：

- **Schema**：统一类型体系（对齐并超过 protobuf）
- **Transport**：单机优先，inproc 指针 + 跨进程 shm 零拷贝（强约束）
- **Runtime**：可预测调度、资源控制、观测、record/replay
- **Tools**：可安装、可运行、可诊断、可回放（闭环体验）
- **Modules**：业务模块只做装配与适配，算法保持纯净

## 3. 完整系统的分层模型（从底到上）

### 3.1 Foundation（Basement）

- core：日志/异常/时间/基础设施
- schema：IDL+codec+reflection+compat+工具
- transport：topic/service/event；inproc+shm；观测
- runtime：component/host/scheduler/trigger/process；观测
- manifest：module/component 清单（清单即类型）
- tools：build/run/graph/doctor/record/replay/monitor

### 3.2 Libraries（算法资产层）

纯 C++ 算法与数据结构层，禁止依赖 transport/runtime；只依赖 core 与必要的数学/几何库。

### 3.3 Modules（业务装配层）

模块结构对齐 Autoware/Apollo：

- setting（config/parameter/health）
- localization
- perception
- prediction
- planning（含 motion/decision/navigation 等）
- control
- map / routing
- simulation / replay

每个 module 内：

- `*_component`：I/O 适配、参数注入、触发策略、生命周期
- `*_processor`：纯业务逻辑（可单测、可离线）

### 3.4 Deployment（部署与运维）

面向工程交付需要：

- install layout：bin/lib/modules/config/logs/record
- 进程编排：按 process_group 划分进程与资源隔离
- 健康检查：心跳、延迟、队列、丢帧、崩溃收集
- 诊断工具：graph/doctor/monitor/trace

## 4. “全系统闭环”场景（验收驱动）

为了避免“底座开发吞掉业务”，以闭环验收驱动实现顺序：

### 4.1 最小运行闭环（已实现雏形）

- setting/config + setting/parameter 发布 snapshot
- planning/motion 消费 snapshot，输出 trajectory/status（可先 stub）

### 4.2 传感器到控制闭环（目标）

- localization/perception 输入 → prediction → planning → control 输出
- 关键验收：帧一致性、端到端延迟、record/replay 可复现

### 4.3 离线仿真/回放闭环（目标）

- record：写入 schema wire format
- replay：按时间戳/帧驱动回放，支持速率控制与跳转
- 关键验收：回放与线上一致（同一版本、同一参数、同一输入）

## 5. 工作拆分（从完整形态倒推）

按"依赖 + 风险 + 复用收益"排序：

### 5.1 Schema（第一优先级）

- IDL v1：字段 id、enum/oneof/map/repeated/optional/default
- Codec v1：二进制 wire format（确定性、可流式解析）
- View/Builder：零拷贝读写（shm loan）
- Compat：破坏性变更检测工具、unknown field 保留策略

### 5.2 Transport（第二优先级）

- inproc：TypedConstPtr 指针传递（强约束）
- shm：loan + view（强约束）
- routed：同进程走 inproc、跨进程走 shm（默认策略）
- 观测：topic 频率/延迟/payload size/丢帧等统计

### 5.3 Runtime（第三优先级，拆分推进）

- component：生命周期与上下文注入
- host：装配、plugin 加载、process_group 组织
- scheduler：优先级队列、线程池隔离、后续帧一致性/DAG
- trigger：timer/topic + 帧语义（FrameContext）
- process：多进程编排、监督、日志分离、健康检查

### 5.4 Tools/Manifest（第四优先级）

- manifest 清单迁移到 schema（清单即类型）
- tools：schema fmt/gen/compat + record/replay + monitor/trace

## 6. 对齐策略总结

- 参考 Apollo：把“强类型 + 零拷贝 + 工具闭环”作为能力上限锚点
- 参考 Autoware：把“模块边界与组件实践”作为业务工程形态锚点
- 参考 ROS2：把“生态与分布式能力”作为可选扩展锚点（不进入主线）

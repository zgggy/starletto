# Starletto Basement 设计方案（Native-only + Schema/Value 统一类型体系）

## 0. 文档定位

这份文档是 Starletto 后续“设计、推演、头脑风暴、开发”的共同基线，覆盖：

- `basement/` 的目标架构与边界（runtime / transport / manifest / tools / schema）
- 统一类型体系（`.schema` + 值文件 + codegen/codec）的能力边界（对齐并超过 protobuf）
- 与当前仓库“已落地代码”的对齐关系（不推倒重来）
- 分阶段实施路线（P0/P1/P2），以及每阶段可执行验收标准

本文档是“最低标准”：任何新能力不得低于 schema/value 体系的统一性与零拷贝要求；任何新代码必须落在新边界上。

## 1. 背景与现状（仓库已落地能力）

### 1.1 已落地（当前主路径）

当前仓库已经具备 Native-only 主路径：

- 构建/运行：`./star build`、`./star run`、`./star install`（Meson/Ninja + native runner）
- 装配：`module.conf`/`component.conf` 驱动装配，plugin 动态注册 `star_register_components`
- 调度：timer/topic trigger 驱动 `process_once()`，支持 priority；`process_group` 已作为调度域隔离
- 安装布局：install-tree（`artifacts/`）可独立运行并产生日志
- 排障工具：`star list/graph/doctor`（含 `--deep` 插件可加载检查）
- 多进程骨架：`star run --multi-process` 按 `process_group` fork/exec
- 零拷贝底座：
  - 单进程：inproc 指针传递（`TypedConstPtr`）
  - 多进程：共享内存 shm 零拷贝（payload 指向共享内存）
  - 多进程默认采用 routed 模式：同组走 inproc，跨组走 shm

### 1.2 已存在但需要收敛/替换的部分

- `ConfValue`（当前用于配置与部分 snapshot 传递）应逐步退出“运行时数据通路”
- 现有 `.schema` + validator + codegen 的落点仍是 `ConfValue`（缺少二进制 wire format 与 typed zero-copy View/Builder）

## 2. 术语与约束（后续沟通统一口径）

### 2.1 术语表

- **Schema（模式）**：类型的结构描述（字段、类型、可选/重复、默认值、约束）
- **Value（值表示）**：类型在内存/运行时的承载形式
- **Snapshot（快照）**：把某一类系统状态在某时刻的“全量”内容封装成一条消息，便于订阅者快速获得一致状态
- **Wire Format（线格式）**：消息在通信链路中真实传输的字节序列（bytes），定义了编码/解码规则
- **Zero-copy（零拷贝）**：接收端拿到的 payload 直接指向发送端写入的共享内存（或同进程共享对象），避免额外 payload 拷贝
- **Loan（借用）**：发送端向 transport “借”一块可写 buffer（通常来自 shm slot），在其上直接构造/编码消息，然后提交（commit）
- **View（视图）**：接收端对一段 bytes 的只读结构化访问接口，不需要把 bytes 反序列化成堆对象
- **Builder（构造器）**：在一段可写 buffer 上构造/编码消息的接口，避免中间对象与额外拷贝

### 2.2 约束（必须遵守）

- Native-only：主路径不考虑 ROS；任何 bridge 必须独立工程且不进入 `libraries/` 与 `basement/runtime`
- 单进程必须指针传递：inproc 消息必须走 `TypedConstPtr` 共享对象，不得走序列化/反序列化
- 多进程必须零拷贝：跨进程消息必须以 shm（或等价机制）零拷贝传递 payload
- Schema 统一性：配置/参数/消息/服务请求响应/事件，必须共享同一个 `.schema` + 值文件 + codegen/codec 体系与演进规则

## 3. 总体目标架构（Basement 分层）

### 3.1 分层总览

Basement 未来拆为五个层级（命名保持现有目录，语义收敛）：

1) **core**：异常、日志、时间、基础类型、内存与工具基础设施  
2) **schema**：统一类型体系（`.schema`、值文件、codec、registry、reflection、compat rules）  
3) **transport**：通信抽象与实现（inproc + shm + 后续的控制面/观测）  
4) **runtime**：组件生命周期、装配、调度、触发、进程编排、运行时上下文  
5) **manifest**：module/component 清单解析、校验、装配信息生成（运行时唯一清单）  
6) **tools**：`star` 与 runner 的工具能力（list/graph/doctor/record/replay/monitor）

其中 schema/value 体系是“系统类型源”；transport/runtime/manifest/tools 都不得绕开这套体系自建类型体系。

### 3.2 Basement 的职责边界

- `basement/runtime` 不定义业务消息结构，只消费 schema codegen 生成的类型（View/Builder/Descriptor）
- `basement/transport` 不关心业务含义，只实现“按 channel + type_id 分发 bytes/loan buffer”
- `basement/manifest` 的配置文件本身也应迁移到 schema 体系（清单即类型）
- `modules/*` 的 component 只做适配与装配，processor 只做纯逻辑

## 4. Schema：统一 `.schema` + 值文件体系（最低标准）

schema/value 体系的定位：Starletto 内部一切“结构化数据”的唯一来源与唯一通路，包括：

- 配置（静态配置、动态参数）
- 消息（topic pub/sub）
- 服务（request/response）
- 事件（event bus）
- 记录回放（record/replay）

### 4.1 Schema 体系的能力边界（对齐 protobuf）

必须具备的“protobuf 等价能力”：

- IDL：message/enum/oneof/map/repeated/optional/default
- 字段 id（等价 protobuf field number）：稳定 tag，支持重命名不破坏兼容
- binary codec：确定性 wire format、跨平台一致、可流式解析
- unknown field 保留：老版本能接收并转发新字段（forward compatibility）
- 兼容演进规则与检查工具：禁止复用字段 id、类型变更规则、reserved id/name
- 反射与 descriptor：运行时可查看字段、类型、默认值、注解
- 多语言潜力：至少保证 wire format 与 IDL 语义可被其它语言实现（即使短期先只实现 C++）

### 4.2 Schema 体系“超过 protobuf”的目标（作为卖点）

- zero-copy first：为 shm/loan 场景提供一等公民的 View/Builder 生成物
- 多 codec 后端：同一 IDL 可配置不同编码（compact / zero-copy-friendly / debug-text）
- 可观测性一体化：可选的 tracing/meta（可关闭），支持链路延迟与统计采集
- 配置与消息统一：配置不是“另一套值体系”，而是 schema 体系的一种表示形式（例如 text codec）

### 4.3 字段 id：为什么必须存在

字段 id 的作用：

- 兼容演进：字段重命名/移动不影响解码
- unknown field：保留未知字段以便转发与回写
- 高性能：编码/解码不依赖字符串字段名
- 长期维护：避免“按名字”造成的隐式破坏

建议支持两种模式：

- 显式 id：schema 作者手写指定（推荐）
- 自动分配：工具按声明顺序分配并写回 schema 文件（降低初期维护成本），但一旦写回就视为冻结

### 4.4 Schema 体系的核心组件（建议 API）

#### 4.4.1 SchemaRegistry

- 以 `TypeId(name, version, hash)` 作为键
- 注册内容不仅包括字段描述，还包括 codec vtable（encode/decode/validate/size）
- 对应现有 `SchemaRegistry` 的升级版

#### 4.4.2 SchemaCodec（wire format）

至少提供：

- `encode(message, Writer)`：写入 bytes（支持写入 loan buffer）
- `decode(View)`：必要时构造对象；优先提供 `View` 直接访问
- `validate(message)`：结构与约束校验（上线可选择关闭或降级）

#### 4.4.3 SchemaView / SchemaBuilder

- View：对 bytes 的只读结构化访问（不拷贝）
- Builder：在 loan buffer 上构造消息（不拷贝）

### 4.5 ConfValue 的迁移策略（不推倒重来）

ConfValue 的最终定位：只用于“人类输入文本”或“debug text codec”。

迁移策略：

1) 把现有配置/参数 snapshot 迁移为 schema typed message（binary + View/Builder）
2) 把 manifest 的 module/component conf 迁移为 schema typed conf（清单即类型）
3) 禁止新模块在消息通路使用 ConfValue（CI/静态检查/编译期约束）
4) 逐步将 `parse_conf_*` 限定为 tools/config 导入路径，不再进入 runtime/transport 的数据通路

## 5. Transport：对齐 Apollo/Cyber 的通信能力边界

### 5.1 单进程（inproc）

目标语义：共享指针传递（payload 0 拷贝），订阅回调获得 `TypedConstPtr`。

约束：inproc 不得走序列化；除非显式要求跨线程队列复制（默认不允许）。

### 5.2 多进程（shm）

目标语义：loan buffer + commit，接收端获得 shm view（payload 0 拷贝）。

需要的 schema 体系支持：

- Builder 直接写入 loan buffer
- View 直接在 shm bytes 上读取字段

### 5.3 路由模式（inproc + shm）

多进程部署时，同组 inproc、跨组 shm，是现阶段的默认策略（避免单进程被 poll 线程拖慢）。

后续可扩展为：

- 自动拓扑分析 + 自动选择 shm topic
- 支持显式 qos/profile（depth、reliability、drop policy）

### 5.4 必备工具能力（对齐 cyber_channel/cyber_monitor）

transport 层必须暴露可观测信息：

- topic 列表、writer/reader 数、频率、payload size、延迟统计（可选）
- 与 Schema TypeId 的绑定关系

## 6. Runtime：对齐 Apollo 运行时语义

### 6.1 Component 模型

组件是普通 C++ 对象，生命周期接口：

- `init()`
- `process_once()`
- `shutdown()`

组件只能通过 `RuntimeContext` 获取系统能力（transport/scheduler/parameter/service/logger/clock）。

### 6.2 Scheduler/Trigger

当前已实现 timer/topic trigger + priority，后续对齐 Apollo 需要：

- 帧级一致性（FrameContext）：同一“帧”内跨组件的顺序与一致性
- DAG 调度：依赖图与 barrier
- 资源感知：线程池隔离、affinity、优先级队列、可选协程

### 6.3 Process orchestration（多进程编排）

目标：从“按 group fork/exec”进化到“可配置的进程拓扑 + 监督 + 健康检查 + 日志分离”。

重要前提：schema typed message + shm zero-copy 完成后，再做更复杂的多进程治理才不会反复返工。

## 7. Manifest：清单即类型（必须迁移到 schema 体系）

### 7.1 现状

module/component 的 `.conf` 目前使用 conf parser + schema 校验，是正确方向，但 value 仍是 ConfValue。

### 7.2 目标

manifest 的结构（module/component/endpoint/qos/trigger）应由 `.schema` 定义，并生成 typed struct：

- `ModuleManifest`
- `ComponentManifest`
- `EndpointDescriptor`

优势：

- 清单自带 field id 与演进规则
- 工具链（list/graph/doctor）直接基于 typed data，不需要 ConfValue

## 8. Tools：以 schema 体系为核心的开发体验

目标：对齐并替代 protobuf+cyber 的常用工具体验：

- `star schema fmt`（schema 格式化）
- `star schema gen`（codegen）
- `star schema compat`（兼容性检查）
- `star graph`（模块/组件/Topic 拓扑）
- `star doctor`（schema/manifest/插件/运行环境诊断）
- `star record/replay`（统一使用 schema wire format）

## 9. 分阶段实施路线（与现有代码对齐）

### P0（已完成）：Native-first 骨架 + 最小闭环

- Meson + star 工具
- manifest 装配 + plugin
- scheduler/trigger 最小闭环
- inproc 指针传递 + 跨进程 shm 零拷贝底座

### P1（进行中）：可部署运行时（单机优先）

已完成：

- install-tree 运行与日志
- list/graph/doctor
- process_group 调度域与多进程骨架
- shm routed：同组指针、跨组 shm

接下来优先级（P1 后半段）：

1) schema typed snapshot：让 `setting.parameter.snapshot`/`setting.config.snapshot` 彻底去掉 dump/parse
2) 进程编排完善：监督、健康检查、资源限制、默认日志规则
3) transport 观测与统计：topic 频率/延迟/丢帧

验收建议：

```bash
./star build
./star run --frames=2 planning
./star run --multi-process --frames=6 planning
./star doctor --deep planning
```

### P2：Schema/value 体系作为系统类型源（完全替代 ConfValue）

目标：

- `.schema` + binary codec + View/Builder 成为唯一通路
- manifest/参数/消息/服务全部迁移到 schema/value 体系
- record/replay/monitor 基于 schema wire format

验收建议：

- 不使用 ConfValue 也能完成：配置加载、参数更新、核心 topic 通信、record/replay

## 10. 关键设计决策（Decision Record）

- 主路径 Native-only，不考虑 ROS；互通以后以 bridge 独立工程实现
- schema/value 体系是“唯一类型体系”，对齐并超过 protobuf；字段 id 必须存在
- 单进程指针传递、跨进程 shm 零拷贝是硬约束
- ConfValue 逐步退出运行时数据通路，最终仅作为文本配置/调试表示

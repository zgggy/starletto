# Basement 子模块拆分索引（Native-only）

目标：把 `basement/` 拆成边界清晰、可独立推进的子模块，并按依赖顺序逐个构建与验收。  
推进原则：先 Schema（`.schema` + 值文件 + codegen/codec）→ 再 Transport（通信）→ 再 Runtime（装配/调度/进程）→ 再 Tools/Manifest 完善闭环。

## 1. 模块分解（最终边界）

- **core**
  - 职责：异常、日志、时间、基础类型、内存与调试设施
  - Spec：[basement_core.md](basement_core.md)
- **schema**
  - 职责：`.schema` + 值文件 + codec + reflection + compat（对齐并超过 protobuf）
  - Spec：[basement_schema.md](basement_schema.md)
- **transport**
  - 职责：topic/service/event 的传输层（inproc 指针、跨进程 shm 零拷贝、观测）
  - Spec：[basement_transport.md](basement_transport.md)
- **runtime**（必须拆分，禁止“大而全 runtime”）
  - 职责：组件生命周期、装配、调度、触发、进程编排、运行时上下文
  - Spec 索引：[basement_runtime_index.md](basement_runtime_index.md)
- **manifest**
  - 职责：module/component 清单解析、校验、装配信息生成；清单本身必须迁移到 schema 体系
  - Spec：[basement_manifest.md](basement_manifest.md)
- **tools**
  - 职责：star/runner + schema 工具链 + graph/doctor/record/replay/monitor
  - Spec：[basement_tools.md](basement_tools.md)

## 2. 依赖顺序（开发推进顺序）

1) **Schema v1**：IDL（含字段 id）+ binary codec + C++ View/Builder + compat 规则与检查  
2) **Transport v1**：inproc 指针传递 + shm loan/view（按 Schema TypeId）+ 基础观测  
3) **Runtime v1**：ComponentHost/ProcessGroup/Trigger/Scheduler 只消费 schema 与 Transport，不自建类型  
4) **Manifest v1**：manifest 清单迁移为 schema 类型（清单即类型）  
5) **Tools v1**：schema fmt/gen/compat + record/replay + monitor/graph 完整闭环  

## 3. 当前仓库与目标边界的映射

- 已存在 `basement/schema/*`、`libraries/common/conf/ConfValue`：它们属于当前 schema 体系的前身，但必须升级为完整的 schema/value + codec 体系  
- 已存在 `basement/transport/native`、`basement/transport/shm`：它们属于 Transport v1，需改为严格以 Schema TypeId + bytes/view 为核心  
- 已存在 `basement/runtime/*`：属于 Runtime v1 雏形，但需要按 spec 进一步拆分并收敛 API  

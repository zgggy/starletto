# Basement / Manifest 设计方案（清单即类型）

Manifest 的职责是：把 workspace 的“模块/组件/端点/触发/调度/部署”信息固化为可校验、可演进、可工具化的清单，并生成 runtime 装配所需的数据结构。

核心原则：**清单即类型**。Manifest 文件自身必须迁移到 schema，不再以 ConfValue 作为长期载体。

## 1. 现状

- 当前 `module.conf`/`component.conf` 已存在并可驱动运行
- ✅ **Schema-typed 主通路**（2026-04-07）：`NativeManifestLoader::load_module/load_component()` 使用 `ModuleConf::from_value()`/`ComponentConf::from_value()`（codegen 生成类型）
- ✅ **死代码清理**（2026-04-07）：删除 ~96 行 ConfValue 风格 helper 函数（get_object/get_string/get_int64/get_bool 等）
- `module.schema` / `component.schema` 已定义 → codegen 生成 `.s.h`
- `runtime_plan_builder.h` 完全消费 typed struct

## 2. 目标

- module/component manifest 由 schema IDL 定义并生成类型
- runtime/transport/tools 消费 typed manifest，不再处理 ConfValue
- manifest 演进具备兼容规则（字段 id、reserved、unknown field）

## 3. 清单模型（最终）

### 3.1 ModuleManifest

包含：

- module info：name/version/install_group
- layout：config_dir/component_dir
- components：component.conf 列表（或直接嵌入 component 描述）
- dependency：modules/libraries

### 3.2 ComponentManifest

包含：

- global：process_group、scheduler_policy、executor_threads
- component：name/type/class/library/static_link/priority
- trigger：type/cycle_ms/task_name/readers（多 topic 同步后续扩展）
- endpoints：writers/readers/commands（统一 EndpointDescriptor）
- parameters：静态参数（schema typed）

### 3.3 EndpointDescriptor（与 Schema/Transport 对齐）

必须包含：

- topic name
- Schema TypeId（或能推导得到）
- qos/profile（depth/drop policy 等）
- record 标记

## 4. Manifest → Runtime 的生成物

Manifest 层必须生成：

- `ComponentLoadPlan`：组件库路径、注册符号、类名
- `ProcessPlan`：按 process_group 的进程划分
- `SchedulePlan`：任务优先级、线程池隔离、资源亲和（对齐 Apollo classic/choreography 的可表达性）
- `TriggerPlan`：trigger 与 topic 的绑定关系

这些 plan 结构必须是 typed（schema 或纯结构体），并且可被 tools 输出为 graph/doctor。

## 5. 迁移策略（从现状到目标）

1) 先让 manifest 的 schema/validator 迁移为 schema IDL（字段 id 引入）
2) 保留 conf 作为“debug_text codec”，允许人类继续编辑
3) runtime/tools 内部只消费 schema typed manifest（通过 schema 解码）

### 5.1 本轮最小落地约束（typed manifest loader）

- 先不重写全部 runtime plan 生成逻辑，只先替换 manifest 的“解析入口”
- `NativeManifestLoader` 必须优先使用生成态：
  - `star::generated::manifest::module::ModuleConf`
  - `star::generated::manifest::component::ComponentConf`
- conf 文件仍由运行时解析为值文件，但 loader 内部要立刻转为 generated typed manifest，再映射到 `ModuleManifest/ComponentManifest`
- 目标：让 runtime 的 manifest 主通路开始依赖 `.schema` 生成类型，而不是手写 `get_string/get_bool/get_object`

### 5.2 本轮最小落地约束（Apollo 术语统一）

- component manifest 主术语统一为：
  - `writers`
  - `readers`
- trigger 配置统一为：
  - `trigger.readers`
- 不保留 `publishers/subscriptions` 兼容字段

## 6. 验收标准

- `star list/graph/doctor` 只依赖 schema typed manifest
- `module.conf/component.conf` 的演进具备 compat 检测
- 新增字段不会破坏旧版本运行（unknown field 保留）

本轮最小验收：

```bash
./star build
./star doctor planning
./star run --frames=2 planning
```

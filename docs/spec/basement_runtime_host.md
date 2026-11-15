# Basement / Runtime / Host 设计方案（装配、加载、进程组）

Host 子模块的职责：把 manifest 描述的组件装配为可运行实例，并与 scheduler/trigger/transport 连接起来。

## 1. 目标

- 装配可重复、可诊断、可观测
- plugin 加载可控（路径解析、符号检查、错误输出）
- process_group 作为部署单位（与 process 子模块配合）
- host 不引入业务语义，只做装配

## 2. 组件模型（Component）

Component 是普通 C++ 对象，最小接口：

- `init()`
- `process_once()`
- `shutdown()`

组件只能通过 RuntimeContext 获取系统能力。

组件配置通过 **typed plan 单轨制** 获取：

- `set_load_plan(ComponentLoadPlan)`：注入组件加载计划（唯一配置来源）
- `load_plan()`：获取当前加载计划指针（可为空）

**已移除**：`set_manifest()` / `manifest()` / `manifest_` 成员（M3 typed plan 深化已完成单轨制迁移）。

## 3. Host 的职责边界

Host 必须负责：

- 加载 ComponentManifest
- 解析库路径并 `dlopen`（或 static link）
- 调用 `star_register_components` 注册类
- 按 `component_class` 创建实例
- 注入 RuntimeContext
- 建立 trigger 与 scheduler 的绑定（task 注册）
- 建立 channel reader/writer 与 component 的 I/O 适配（由 component 自身完成）

Host 不做：

- schema/类型定义（归 Schema）
- transport 具体实现（归 Transport）
- 业务拓扑推理（归 Manifest/Tools）

## 4. 路径解析规则（安装态优先）

组件库解析顺序建议固定：

1) `component.conf` 同目录
2) `module_dir` 下
3) build-tree 推导（开发态）
4) `STAR_LIB_PATH`
5) 系统动态库路径

目标：install-tree 可独立运行且不回退到源码树。

## 5. process_group 与 host 的关系

长期目标：

- 一个 worker 进程只包含一个或多个 process_group（默认一个）
- host 只装配“本进程内”的组件集合
- 跨组通信通过 transport 的 interproc（shm）完成

## 6. 触发与调度绑定（最小规则）

- timer trigger：周期任务注册到 scheduler（period_ms）
- topic trigger：reader 回调触发 `scheduler.notify(task)`
- priority：任务进入多优先级队列（避免优先级反转）

### 4.1 typed plan 直接消费（✅ 已完成）

目标：`NativeComponentHost::load(ComponentLoadPlan)` 不再设置 `ComponentManifest` 桥接。

**已完成迁移（3 步全部落地）：**

1. ✅ `Component` 增加 `load_plan()` 接口
2. ✅ 所有组件从 `manifest()->output_endpoints/input_endpoints` 迁移到 `load_plan()->outputs/inputs`
   - motion_component：4 处 resolve 方法
   - parameter_component：3 处 resolve 方法 + config_file_path 解析
   - config_component：3 处 resolve 方法 + config_file_path 解析
3. ✅ `NativeComponentHost::load()` 移除 `set_manifest()` 调用、`manifests_` 成员、`register_manifest_endpoints()` 方法
4. ✅ `ComponentHost` 接口从 `load(ComponentManifest)` 改为 `load(ComponentLoadPlan)`
5. ✅ 全代码库 `grep manifest()` → 0 matches

关键约束：
- `EndpointPlan`（name/topic/type_name）与 `EndpointDescriptor` 完全兼容
- `ComponentLoadPlan.outputs/inputs` 与 `ComponentManifest.output_endpoints/input_endpoints` 字段一一对应
- 组件的 `resolve_*_topic_name()` / `resolve_*_type_name()` / `resolve_*_root_path()` 全部优先走 `load_plan()` 路径，无 plan 时返回默认值

### 4.2 未来扩展
- 多 topic 同步触发（数据驱动一致性）
- 帧语义（FrameContext）
- DAG barrier

## 7. auto_configure：conf 驱动的零样板组件（终态）

### 7.1 结论

`auto_configure()` 已在 `init()` 之前按 `ComponentLoadPlan` 建好全部端口：

- 非 trigger inputs → `read_latest<T>(port)` 的缓存 reader（含 QoS depth/drop_policy、decode_policy）
- trigger inputs → 不建缓存 reader；消息由 Trigger 管线作为 `FrameInputs` 随
  `process_once()` 传入
- outputs → `write(port, msg)` 的 writer（含 `record=true` 的录制开关、bidirectional）
- services → `RpcClient`/`RpcServer`

因此组件不再需要 `declare_reader`/`declare_writer`。这两个 API 与 Component 层的
`declare_delivery_writer`/`write_with_delivery` 已删除（2026-11-14），避免“conf 一套、
init 里再覆盖一套”的双轨装配。

### 7.2 组件侧唯一写法

```cpp
class ControlComponent final : public star::runtime::Component {
  public:
    auto process_once(const FrameInputs& inputs) -> bool override {
        // planning_trajectory 是 trigger 端口：本次触发的消息在 inputs 里
        if (auto traj = trigger_input<TrajectoryOutput>(inputs, "planning_trajectory")) {
            latest_trajectory_ = std::move(traj);
        }
        if (not latest_trajectory_) {
            return true;
        }
        write("command", controller_.compute(*latest_trajectory_));
        return true;
    }
};
```

端口名以 `component.conf` 为唯一声明源；`init()` 只做一次性初始化。

### 7.2.1 类型化输入（可选，推荐）

`TypedComponent<Derived, Inputs>` 把 `FrameInputs` 适配成组件自己的输入结构；
框架仍然只调用唯一的 `process_once(FrameInputs)`，组件业务方法可以有不同参数。
端口名 → 类型的映射写在 `make_inputs()` 一次，业务方法不再出现模板+字符串取值。
见 `basement/runtime/component/typed_component.h`。

### 7.3 Host 调用顺序

```
NativeComponentHost::prepare_components():
  for each component:
    set_context() / set_load_plan()   （load 时）
    auto_configure()                  ← 按 plan 建 reader/writer/service
    init()                            ← 用户只做非端口初始化
```

### 7.4 约束

- `component.conf` 的 `readers`/`writers` 的 `type`/`msg_type` 必须有效；缺失时
  解码路径不可用。
- trigger 端口只能通过 `process_once(inputs)` 的 `trigger_input<T>()` 读取；
  非 trigger 端口用 `read_latest<T>()`。两套入口不要混用。
- trigger 消息在触发线程上做不可变快照（shm 拷贝），保证 process_once 看到的就是
  触发它的那条消息。

## 8. 验收标准

- host 能输出完整装配日志：加载的 plan、库路径、类名、任务名、process_group
- `doctor --deep` 能复用 host 的加载路径规则
- build-tree 与 install-tree 行为一致
- ✅ typed plan 单轨制：组件不依赖 `ComponentManifest`，全走 `ComponentLoadPlan`
- ✅ multi-process worker 进程正确消费 `load_plan()`（parameter/config/motion 全部验证通过）
- ✅ **auto_configure 零样板模式**（2026-04-05 验证通过）：
  - `./star build` 零错误编译
  - `./star run --frames=5 motion/component.conf control/pid/component.conf` 单进程闭环正常
  - `./star run --frames=5 --multi-process ...` 双进程 SHM 路由正常（e2e_latency_us: 300~430μs）
  - MotionComponent: 只消费三路 trigger；日志不是装配必需项
  - ControlComponent: init() 仅保留日志，轨迹走 trigger_input

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

## 7. auto_configure：conf 驱动的零样板组件（P0 增强）

### 7.1 目标

消除用户在 `init()` 中手写 `declare_reader` / `declare_writer` 的样板代码。**component.conf 中声明的 readers/writers 应自动转化为可用的 reader/writer 实例**，用户只需关注业务逻辑。

### 7.2 设计原则

1. **conf 是唯一声明源**：readers/writers 在 component.conf 中定义，不再需要在 C++ 中重复
2. **向后兼容**：已有的 `declare_reader<T>(name, cb)` / `declare_writer<T>(name)` API 保留，可在 `init()` 中覆盖默认行为
3. **两种使用模式**：
   - **轮询模式（默认）**：auto_configure 自动创建"缓存最新消息"的 reader，用户在 `process_once()` 中通过 `read_latest<T>(name)` 获取
   - **回调模式（高级）**：用户在 `init()` 中调用 `declare_reader<T>(name, cb)` 覆盖默认行为

### 7.3 接口设计

```cpp
class Component {
public:
    // Host 调用顺序: set_load_plan() → auto_configure() → init() → [scheduler start]

    // 新增：从 load_plan 自动创建所有 readers/writers
    void auto_configure();

    // 新增：读取某个 reader endpoint 的最新消息（类型安全）
    template <typename _Msg>
    auto read_latest(const std::string& endpoint_name) -> std::shared_ptr<const _Msg>;

    // 新增：发布到某个 writer endpoint（类型安全，write 的别名）
    template <typename _Msg>
    void publish(const std::string& endpoint_name, const _Msg& msg);

    // 已有 API（保留不变）
    template <typename _Msg>
    void declare_reader(const std::string& name, std::function<void(std::shared_ptr<const _Msg>)> callback);

    template <typename _Msg>
    void declare_writer(const std::string& name);

    template <typename _Msg>
    void write(const std::string& name, const _Msg& msg);
};
```

### 7.4 auto_configure 行为规范

```
auto_configure() 执行流程:
1. 检查 plan_ 是否有效，无效则返回
2. 遍历 plan_->inputs (readers):
   a. 对每个 EndpointPlan { name, topic, type_name }:
      - 创建 TopicDescriptor (topic + type_name)
      - 创建 Reader，callback = 存储 MessageView 到 latest_messages_[name]
      - 缓存 reader 到 readers_[name]
3. 遍历 plan_->outputs (writers):
   a. 对每个 EndpointPlan { name, topic, type_name, record }:
      - 创建 TopicDescriptor (topic + type_name)
      - 创建 Writer，缓存到 writers_[name]
      - 若 record=true，启用 recorder
```

**latest_messages_ 存储格式**：
- key: endpoint name（来自 conf）
- value: 最新的 `MessageView`（包含 TypedConstPtr + encode_binary 闭包）

**read_latest<T>(name) 行为**：
1. 从 latest_messages_[name] 取出最新 MessageView
2. 尝试 `msg.data.as<T>()` （inproc 指针路径）
3. 若失败且 T 有 `from_binary()`，尝试 SHM 二进制解码路径
4. 返回 `shared_ptr<const T>` 或 nullptr

### 7.5 用户组件编写指南（新）

#### 模式 A：最小样板（推荐，轮询式）

```cpp
class MotionComponent : public star::runtime::Component {
  public:
    // init() 可以为空！auto_configure 自动完成所有 reader/writer 声明

    bool process_once() override {
        auto param_snap = read_latest<ParameterSnapshot>("parameter_snapshot");
        auto config_snap = read_latest<ConfigSnapshot>("config_snapshot");
        auto output = processor_.process({});
        publish("trajectory", output);       // 类型安全的发布
        return true;
    }

  private:
    MotionProcessor processor_;
};
```

#### 模式 B：回调式（需要自定义处理逻辑时）

```cpp
class ControlComponent : public star::runtime::Component {
  public:
    void init() override {
        // 用自定义 callback 覆盖 auto_configure 的默认"缓存"行为
        declare_reader<TrajectoryOutput>("planning_trajectory",
            [this](auto traj) { on_trajectory(traj); });
        // writer 不需要覆盖，auto_configure 已处理
    }
    bool process_once() override {
        if (not latest_trajectory_) { return true; }
        auto cmd = controller_.compute(*latest_trajectory_);
        publish("command", cmd);
        return true;
    }
};
```

### 7.6 Host 调用契约变更

```
NativeComponentHost::start() 调用顺序变更:

Before:
  for each component:
    c->init()          ← 用户在此手动 declare_reader/declare_writer

After:
  for each component:
    c->auto_configure()  ← 新增：自动从 plan 创建 reader/writer
    c->init()            ← 用户可选覆写或添加额外逻辑
```

### 7.7 兼容性规则

- **不破坏已有组件**：motion_component.h / control_component.h 中现有的 `declare_reader`/`declare_writer` 调用在 `init()` 中执行，会覆盖 `auto_configure` 创建的同名 reader/writer
- **conf 必须包含有效的 type_name/msg_type**：`read_latest<T>()` 依赖运行时类型信息做 decode fallback；若 conf 缺少 type 信息，SHM 路径降级为指针路径
- **`publish()` 与 `write()` 等价**：`publish` 是 `write` 的别名，语义更清晰

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
  - MotionComponent: **init() 完全删除**，仅保留 process_once() + read_latest<>() + publish()
  - ControlComponent: init() 仅保留 declare_reader 回调，**declare_writer 被 auto_configure 替代**
  - 向后兼容：旧式 declare_reader/declare_writer 在 init() 中仍可覆盖 auto_configure 行为

# 02 · 分层与模块职责

## 1. 分层图与依赖方向

```mermaid
flowchart TB
    subgraph L1["① 工具链 star"]
        STAR["build · run · list · graph · doctor<br/>monitor · record/replay · schema · check"]
    end

    subgraph L2["② apps 宿主"]
        RUNNER["native_runner<br/>装配组件、编排多进程"]
        TOOLS["param demo / record_replay / schema_codec"]
    end

    subgraph L3["③ modules 业务组件"]
        MOD_SET["setting<br/>config · parameter"]
        MOD_PLAN["planning<br/>motion"]
        MOD_CTRL["control<br/>pid"]
    end

    subgraph L4["④ basement 底座"]
        direction TB
        RT["runtime<br/>生命周期 · 调度 · 触发器 · 进程编排 · 健康"]
        TR["transport<br/>inproc · shm · routed · record"]
        SC["schema<br/>IDL · codegen · 编解码 · 校验"]
        CO["core<br/>日志 · 异常 · 时钟 · 内存所有权"]
        RT --> TR --> SC --> CO
    end

    subgraph L5["⑤ libraries 算法资产（未接线）"]
        LIB["element 领域类型 · method 规划算法<br/>machine 行为树 · common 数学"]
    end

    subgraph L6["⑥ tests"]
        TST["17 套 112 用例"]
    end

    STAR --> RUNNER
    RUNNER -->|"dlopen 插件"| L3
    L3 --> L4
    LIB -.->|"只允许依赖 core"| CO
    TST --> L4
```

**四种依赖关系，规则不同**：

| 关系 | 规则 | 为什么 |
|---|---|---|
| 上层 → 下层 | 允许 | 上层是业务/装配，下层是能力 |
| 下层 → 上层 | **禁止** | 底座不能知道"什么是轨迹" |
| `modules` → `basement` | 允许 | 业务通过 `Component` 基类使用通信能力 |
| `libraries` → `basement` | **只准依赖 `core`** | 算法库必须能在没有通信/运行时的环境里单独编译、单独测试 |

最后一条是硬约束：`libraries` 里的算法如果依赖了 `transport`，就意味着它不能被离线跑、不能在仿真里跑、不能给别的项目复用。

## 2. 每个模块做什么

### 2.1 底座四层（系统侧）

```mermaid
flowchart LR
    subgraph R["runtime"]
        R1["Component 基类<br/>声明端口、收发消息"]
        R2["NativeScheduler<br/>按优先级/周期调度任务"]
        R3["SyncTrigger / TimerTrigger<br/>决定何时跑一次"]
        R4["ProcessOrchestrator<br/>fork/exec 多进程"]
        R5["HealthChecker / ResourceMonitor<br/>健康与资源观测"]
    end
    subgraph T["transport"]
        T1["MessageBus 通道<br/>publish/subscribe"]
        T2["ServiceBus 服务<br/>request/response"]
        T3["Native / Shm / Routed<br/>三套实现"]
        T4["Recorder / Replayer<br/>STARREC 录放"]
    end
    subgraph S["schema"]
        S1[".schema IDL<br/>类型定义"]
        S2["codegen<br/>生成 C++ 类型"]
        S3["BinaryCodec<br/>变长二进制编解码"]
        S4["FixedLayoutCodec<br/>定长布局，供 shm 零拷贝"]
        S5["SchemaRegistry<br/>运行时类型注册表"]
    end
    subgraph C["core"]
        C1["日志 LoggerManager"]
        C2["异常三类语义"]
        C3["TypedConstPtr<br/>类型擦除的智能指针"]
        C4["futex_compat<br/>跨平台等待唤醒"]
        C5["FdGuard"]
    end
    R --> T --> S --> C
```

| 模块 | 负责 | 不负责 |
|---|---|---|
| `core` | 日志、异常分类、类型擦除指针、时钟、fd RAII | 任何业务概念 |
| `schema` | 类型定义与编解码、兼容性检查 | 数据怎么传输 |
| `transport` | 把字节/指针从一个组件送到另一个组件 | 字节里是什么 |
| `runtime` | 什么时候、在哪个线程、装配哪些组件去跑 | 组件内部算法 |

### 2.2 业务模块（当前 4 个组件）

```mermaid
flowchart LR
    CFG["<b>setting/config</b><br/>in: files/*.conf<br/>out: setting.config.snapshot<br/>周期 1000ms"]
    PAR["<b>setting/parameter</b><br/>in: parameter/config<br/>out: setting.parameter.snapshot<br/>+ 3 个参数服务"]
    MOT["<b>planning/motion</b><br/>in: config.snapshot + parameter.snapshot<br/>out: planning.trajectory<br/>SyncTrigger：两路到齐才跑"]
    PID["<b>control/pid</b><br/>in: planning.trajectory<br/>out: control.command<br/>数据驱动：来一帧跑一次"]

    CFG -->|"setting.config.snapshot"| MOT
    PAR -->|"setting.parameter.snapshot"| MOT
    MOT -->|"planning.trajectory"| PID
    PID -->|"control.command"| REC["STARREC 录制"]
```

| 组件 | 输入 | 处理 | 输出 | 触发方式 |
|---|---|---|---|---|
| `setting/config` | 5 个 `.conf` 文件 | 解析成 `ConfigSnapshot` | `setting.config.snapshot` | 1000ms 定时 |
| `setting/parameter` | 参数配置文件 | 维护 typed 参数表，响应 get/set/list 服务 | `setting.parameter.snapshot` | 1000ms 定时 |
| `planning/motion` | 上面两个快照 | 读参数 → 生成轨迹（**当前是玩具实现**） | `planning.trajectory` | 两路快照都到齐 |
| `control/pid` | `planning.trajectory` | PID 算油门/刹车/转向 | `control.command`（并录制） | 每来一帧 |

## 3. 一个业务模块的内部结构

```mermaid
flowchart TB
    subgraph PLUGIN["libmotion.so（插件）"]
        REG["register.cc<br/>导出 star_register_components"]
        COMP["motion_component.h<br/><b>Component</b><br/>生命周期 + 端口 + 参数解码"]
        PROC["motion_processor.h<br/><b>Processor</b><br/>纯算法"]
        CONF["component.conf<br/>名字/库/类/端口/触发"]
    end
    REG --> COMP --> PROC
    CONF -.->|"运行时读取"| COMP
```

三个文件各司其职：

| 文件 | 谁写 | 内容 | 改动频率 |
|---|---|---|---|
| `component.conf` | 模块作者 | 声明式配置：组件名、类名、端口、触发策略 | 接线/调参时改 |
| `register.cc` | 模块作者 | 约 3 行：把组件类注册进 registry | 几乎不变 |
| `*_component.h` | 模块作者（系统侧） | 生命周期钩子、端口声明、消息类型转换 | 端口变化时改 |
| `*_processor.h` | 算法工程师 | 纯算法 | **每天改** |

> 这个拆分是好的，但"样板量"仍然可观。这就是 [组件开发指南](../component_guide.md) 要解决的问题。

## 4. 为什么要分这么多层

对算法工程师来说，最直接的收益是**可测试性**：

```mermaid
flowchart LR
    A["你的算法<br/>process(input) -> output"] --> B["离线单测<br/>不需要启动系统"]
    A --> C["离线回放<br/>用录制的数据跑"]
    A --> D["在线运行<br/>组件自动装配"]
```

如果算法直接写在 Component 里、直接调总线，那它就只能在线跑，出一帧错数据要起整套系统才能复现。分层的目的就是把"调算法"和"跑系统"解耦。

# 系统架构设计

> 2026-09-27 更新：图改用 Mermaid 并对齐本轮整改后的实际状态
> （分层归位、纯插件装配、codegen 迁 custom_target、CI 门禁 10 项）。
> 前置文档：[架构评审与整改](arch_review_20260926.md)。

## 分层架构与依赖方向

```mermaid
flowchart TB
    subgraph STAR["工具链 star"]
        direction LR
        CLI["build · run · list · graph · doctor<br/>monitor · param · schema · record/replay"]
        CHECK["check：10 项 CI 门禁"]
    end

    subgraph APPS["apps/ —— 宿主与工具（纯插件装配，不含组件代码）"]
        direction LR
        RUNNER["native_runner<br/>app.h 48 行声明 + app.cc"]
        TOOLS2["param demo · record_replay<br/>schema_codec"]
    end

    subgraph MODULES["modules/ —— 业务组件（conf 声明，插件交付）"]
        direction LR
        SETTING["setting<br/>config · parameter"]
        PLANNING["planning<br/>motion"]
        CONTROL["control<br/>pid"]
        SO["libconfig.so · libparameter.so<br/>libmotion.so · libpid.so"]
    end

    subgraph BASEMENT["basement/ —— 运行时底座（严格单向依赖）"]
        direction TB
        RUNTIME["runtime<br/>组件模型 · 调度器 · 触发器<br/>进程编排 · 健康观测"]
        TRANSPORT["transport<br/>Native inproc · Shm 4 槽环形 + seqlock · Routed"]
        SCHEMA["schema<br/>.schema IDL · codegen · BinaryCodec · FixedLayoutCodec"]
        CORE["core<br/>日志 · 异常 · 时钟 · TypedConstPtr · futex_compat"]
        RUNTIME --> TRANSPORT
        TRANSPORT --> SCHEMA
        SCHEMA --> CORE
    end

    subgraph LIBRARIES["libraries/ —— 算法资产（整层未接线）"]
        ALGO["element · method · machine · common<br/>约 3500 行，等路线图模块接线<br/>接线前置：声明 OSQP 依赖"]
    end

    TESTS["tests/ —— 17 套 112 用例<br/>transport 48 · schema 14 · runtime 50"]

    STAR --> APPS
    APPS -->|"dlopen lib*.so"| MODULES
    MODULES --> BASEMENT
    LIBRARIES -.->|"只准依赖 core"| CORE
    TESTS --> BASEMENT
    CHECK -.-> TESTS
```

分层约束：`libraries` 只依赖 `core`；`runtime` 不定义业务消息；`transport` 不理解业务含义；
`apps` 不包含任何组件实现（配置驱动加载）。

## 数据流（`star run` 真实拓扑，4 组件 4 通道）

```mermaid
flowchart LR
    subgraph G_SETTING["process_group：setting"]
        CONFIG["ConfigComponent<br/>libconfig.so"]
        PARAM["ParameterComponent<br/>libparameter.so"]
    end
    subgraph G_PLANNING["process_group：planning"]
        MOTION["MotionComponent<br/>libmotion.so"]
    end
    subgraph G_CONTROL["process_group：control"]
        PID["ControlComponent<br/>libpid.so"]
    end
    REC["STARREC 录制<br/>record / replay"]

    CONFIG -->|"① setting.config.snapshot"| MOTION
    PARAM -->|"② setting.parameter.snapshot"| MOTION
    MOTION -->|"③ planning.trajectory"| PID
    PID -->|"④ control.command"| REC
    MOTION -.->|"record=true"| REC
```

投递路径由 `ShmRoutedMessageBus` 自动选路：

- **同 process_group 内**：inproc，`TypedConstPtr` 指针直传，零序列化
- **跨 process_group**：shm，4 槽环形缓冲 loan/view 零拷贝，读侧 seqlock 复核防撕裂；
  SHM 初始化失败自动降级 inproc
- record 标记的通道写入 STARREC 格式，`star replay` 按时间戳回放

## 构建与代码生成流

```mermaid
flowchart TB
    SRC["*.schema（源码树，唯一类型源）"]
    GEN["build/ 镜像路径下的 .s.h<br/>不入库，改 .schema 自动重生成"]
    DEP["schema_dep / setting_dep<br/>sources: 挂接生成目标"]
    BH["basement 头文件"]
    MH["modules 头文件 + register.cc<br/>→ lib*.so 插件"]
    T["tests/ 17 套用例"]
    CHK["star check：10 项门禁<br/>build → schema-clean → schema-lint<br/>→ schema-compat 对比 git HEAD<br/>→ unit-test → selftest → doctor → list"]

    SRC -->|"custom_target 显式构建依赖"| GEN
    GEN --> DEP
    DEP --> BH
    DEP --> MH
    DEP --> T
    BH --> CHK
    MH --> CHK
    T --> CHK
```

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 构建系统 | Meson | `custom_target` 声明 codegen 依赖、跨平台；schema 生成物不入库防漂移 |
| IDL 格式 | 自研 .schema | 零拷贝硬约束，protobuf 不满足 inproc 指针 + shm loan/view 的统一抽象 |
| 代码组织 | Header-only（库层） | 用户偏好 .h 组织；apps 例外（app.h 已拆 app.cc，无 header-only 理由） |
| 组件交付 | conf 声明 + dlopen 插件 | apps 与组件解耦，`doctor --deep` 可校验插件；入口约定 `star_register_components(ComponentRegistry*)` |
| 错误模型 | 运行时状况用状态码 / 编程错误用异常 | 消除「状态码被异常化、又被调度层降级为日志」的反转链 |
| 日志系统 | 自定义宏 + LoggerManager | 零依赖、多 sink 可切换 |
| 进程编排 | fork/exec + 参数传递 | 简单可靠、无需守护进程 |

**关联 spec**：[system_vision.md](spec/system_vision.md)、[basement_design.md](spec/basement_design.md)

# 01 · 系统全景与当前实现

## 1. 一个完整的自动驾驶系统应该有什么

```mermaid
flowchart LR
    subgraph INPUT["输入"]
        S1["相机"]
        S2["激光雷达"]
        S3["毫米波雷达"]
        S4["GNSS / IMU"]
        S5["高精地图"]
    end

    subgraph PER["感知 perception"]
        P1["检测 detect"]
        P2["分割 segment"]
        P3["跟踪 track"]
    end

    subgraph LOC["定位 localization"]
        L1["多传感器融合"]
        L2["地图匹配"]
    end

    subgraph PRED["预测 prediction"]
        R1["障碍物行为预测"]
    end

    subgraph PLAN["规划 planning"]
        N1["路由 routing<br/>从 A 到 B 走哪条路"]
        N2["决策 decision<br/>遇到障碍物怎么处理"]
        N3["路径 path<br/>几何上怎么绕"]
        N4["速度 speed<br/>快慢怎么走"]
    end

    subgraph CTRL["控制 control"]
        C1["轨迹跟踪 PID/MPC"]
        C2["执行器指令"]
    end

    S1 & S2 & S3 --> P1 --> P2 --> P3
    S4 --> L1
    S5 --> L2
    L1 --> L2
    P3 --> R1
    L2 --> R1
    R1 --> N1 --> N2 --> N3 --> N4
    N4 --> C1 --> C2
```

这张图是**业务形态**：每一格对应一个算法问题。它和 Starletto 无关，任何自动驾驶系统都是这个形状。

## 2. Starletto 当前实现了哪些格

```mermaid
flowchart TB
    subgraph DONE["✅ 已实现（可运行）"]
        SETTING["setting 配置与参数<br/>读 .conf、对外提供 typed 参数服务"]
        MOTION["planning/motion<br/>输出 20 点轨迹（<b>玩具级示例</b>）"]
        PID["control/pid<br/>PID 跟踪轨迹 → 控制指令"]
    end

    subgraph PARTIAL["🟡 有骨架、无算法"]
        MACHINE["planning 行为树框架<br/>SL/TS 相位节点全为空壳"]
        LIBS["libraries 算法资产<br/>A*/Hybrid A*/piecewise jerk/Frenet/ST 图<br/>约 3500 行，<b>未接线</b>"]
    end

    subgraph TODO["⬜ 规划中"]
        PERC["感知"]
        LOCA["定位"]
        PRED["预测"]
        MAP["地图与路由"]
    end

    subgraph BASE["✅ 底座（已完成）"]
        direction LR
        RT["runtime"] --> TR["transport"] --> SC["schema"] --> CO["core"]
    end

    SETTING --> MOTION --> PID
    MACHINE -.-> MOTION
    LIBS -.-> MOTION
    PERC -.-> MOTION
    LOCA -.-> MOTION
    PRED -.-> MOTION
    MAP -.-> MACHINE

    MOTION --> BASE
    PID --> BASE
    SETTING --> BASE
```

**一句话结论**：底座是工程级完成度，业务是骨架级。`planning/motion` 存在的意义是**演示一个组件该怎么写**，它自己产出的轨迹是正弦扰动的假数据。

## 3. 算法应该写在哪一格

这是对算法工程师最重要的一个问题。每个业务模块内部都分成两半：

```mermaid
flowchart LR
    subgraph MODULE["一个业务模块（例：planning/motion）"]
        direction TB
        COMP["<b>Component</b><br/>适配层<br/>收发消息、读配置、管生命周期<br/>—— 系统侧代码"]
        PROC["<b>Processor</b><br/>纯算法<br/>输入结构体 → 输出结构体<br/>—— 算法侧代码"]
    end
    IN["上游消息<br/>参数快照"] --> COMP
    COMP -->|"普通 C++ 结构体"| PROC
    PROC -->|"普通 C++ 结构体"| COMP
    COMP --> OUT["下游消息<br/>轨迹"]

    style PROC fill:#1a3a2a,stroke:#34d399,color:#c9d6e8
    style COMP fill:#1a2a3a,stroke:#22d3ee,color:#c9d6e8
```

- **Processor 是你的地盘**：签名类似 `auto process(const MotionInput&) -> TrajectoryOutput`，纯函数，不碰网络、不碰线程、不碰共享内存，可以离线单元测试。
- **Component 是适配层**：把总线上的消息变成 `MotionInput`，调 Processor，把结果发出去。样板代码多，但模式固定。

> 这条边界是这套系统对算法工程师最重要的承诺：**算法代码零系统依赖**。反过来说，它也是当前易用性痛点的来源——Component 那半边要写的东西不少。详见 [组件开发指南](../component_guide.md)。

## 4. 名词速查（系统侧黑话）

| 词 | 白话解释 | 在 Starletto 里对应 |
|---|---|---|
| **进程 process** | 一个独立运行的程序，有自己独立的内存，进程之间不能直接读对方变量 | `star run --multi-process` 会起多个进程 |
| **线程 thread** | 同一进程内的多条执行流，共享同一份内存 | 调度器 worker、每个订阅者的轮询线程 |
| **通道 channel** | 一条命名的话题，发布者往里写，订阅者从里读 | `/planning/trajectory`、`/control/command` |
| **零拷贝** | 数据只写一次内存，读的一方直接看那块内存，不复制 | 跨进程走 shm；同进程直接传指针 |
| **共享内存 shm** | 一块能被多个进程同时映射的内存 | `/dev/shm` 里的段，4 槽环形缓冲 |
| **插件 plugin** | 编译成独立 `.so` 的代码，运行时才加载进来 | `libmotion.so` 等 4 个 |
| **序列化** | 把内存里的对象变成字节流，才能跨进程/存盘 | `encode_binary` / `from_binary` |
| **QoS** | 投递策略：保留几条、丢了怎么办 | `depth` / `KEEP_LAST` / `DROP_NEWEST` |

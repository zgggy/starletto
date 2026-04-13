# 系统架构设计

## 分层架构（ Basement 6 层）

```
┌─────────────────────────────────────────────┐
│              Modules (业务层)                │
│   planning / control / setting / perception  │
├─────────────────────────────────────────────┤
│              Libraries (算法层)              │
│   math / io / blackboard / geometry         │
├───────────┬───────────┬─────────────────────┤
│  Manifest │  Tools    │    Runtime          │
│  清单解析  │ CLI/工具  │ 组件/调度/进程       │
├───────────┼───────────┼─────────────────────┤
│        Transport (通信层)                    │
│   inproc 指针 / shm 零拷贝 / routed 路由     │
├───────────┴───────────┴─────────────────────┤
│          Schema (类型层)                     │
│   .schema IDL / codegen / codec / registry   │
├─────────────────────────────────────────────┤
│            Core (基础设施)                   │
│   日志 / 异常 / 时间 / 类型 / 内存           │
└─────────────────────────────────────────────┘
```

## 数据流

### 单进程（inproc）

```
Component A ──write(TypedConstPtr)──→ MessageBus ──callback(TypedConstPtr)──→ Component B
                                    ↓
                              TopicStats (QoS/p99)
```

### 多进程（shm）

```
Component A ──loan(shm)──→ SHM Bus ──view(shm)──→ Component B (另一进程)
                 ↑               ↓
            Route Plan    ProcessOrchestrator
```

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 构建系统 | Meson | Python 原生、run_command 灵活 |
| IDL 格式 | 自研 .schema | 零拷贝硬约束，protobuf 不满足 |
| 代码组织 | Header-only | 用户偏好 .h 文件 |
| 日志系统 | 自定义宏 + LoggerManager | 零依赖、多 sink 可切换 |
| 进程编排 | fork/exec + 参数传递 | 简单可靠、无需守护进程 |

**关联 spec**：[system_vision.md](file:///home/zgy/projects/starletto/document/spec/system_vision.md)、[basement_design.md](file:///home/zgy/projects/starletto/document/spec/basement_design.md)

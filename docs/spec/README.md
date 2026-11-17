# Starletto 规范与方案索引（Spec）

本目录存放 Starletto 的“开发方案、架构边界、接口契约、演进规则”类文档。  
后续所有设计讨论与实现应以这些文档为准，任何偏离必须先更新 spec。

## 入口

- Basement 总方案（总览）：[basement_design.md](basement_design.md)
- 自动驾驶系统完整形态推演（对齐 CyberRT/Autoware/ROS2）：[system_vision.md](system_vision.md)

## Basement 子模块（逐个构建）

- Schema（`.schema` + 值文件 + codegen/codec，第一优先级）：[basement_schema.md](basement_schema.md)
- Transport（inproc + shm + 观测）：[basement_transport.md](basement_transport.md)
- Runtime（拆分后的子模块索引）：[basement_runtime_index.md](basement_runtime_index.md)
- Manifest（清单即类型）：[basement_manifest.md](basement_manifest.md)
- Tools（star/runner/诊断/record/replay）：[basement_tools.md](basement_tools.md)
- Core（日志/异常/时间/基础设施）：[basement_core.md](basement_core.md)

## 联调命令说明（2026-11-15）

`planning.motion` 的主 trigger 现在需要三路输入：`/perception/prediction`、
`/localization/pose`、`/control/chassis`。本目录早期验收记录里的
`star run planning` 只加载 planning 模块，没有上游时不会出帧，属于历史命令；
完整假上游联调命令见根目录 README 的“假上游联调”，或
`modules/planning/motion/README.md`。

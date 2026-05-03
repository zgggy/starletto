# Starletto 规范与方案索引（Spec）

本目录存放 Starletto 的“开发方案、架构边界、接口契约、演进规则”类文档。  
后续所有设计讨论与实现应以这些文档为准，任何偏离必须先更新 spec。

## 入口

- Basement 总方案（总览）：[basement_design.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_design.md)
- Basement 子模块拆分索引（按模块推进）：[basement_index.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_index.md)
- 自动驾驶系统完整形态推演（对齐 CyberRT/Autoware/ROS2）：[system_vision.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/system_vision.md)

## Basement 子模块（逐个构建）

- Schema（`.schema` + 值文件 + codegen/codec，第一优先级）：[basement_schema.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_schema.md)
- Transport（inproc + shm + 观测）：[basement_transport.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_transport.md)
- Runtime（拆分后的子模块索引）：[basement_runtime_index.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_runtime_index.md)
- Manifest（清单即类型）：[basement_manifest.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_manifest.md)
- Tools（star/runner/诊断/record/replay）：[basement_tools.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_tools.md)
- Core（日志/异常/时间/基础设施）：[basement_core.md](file:///home/zgy/projects/starletto/00_hub/05_docs/spec/basement_core.md)

# 模块划分

## 三工程边界

| 工程 | 目录 | 职责 | Spec 文档 |
|------|------|------|-----------|
| basement | `basement/` | 运行时底座 | basement_*.md |
| libraries | `libraries/` | 通用算法库 | （待创建） |
| modules | `modules/` | 业务模块 | （待创建） |

## Basement 内部模块

| 模块 | 目录 | 成熟度 | 核心文件 |
|------|------|--------|----------|
| core | basement/core/ | 95% | logger/, iclock.h, exception.h |
| schema | basement/schema/ | 100% | schema_codegen.py, binary_codec.h |
| transport | basement/transport/ | 95% | native_message_bus.h, shm_message_bus.h |
| runtime | basement/runtime/ | 88% | component.h, runtime_factory.h |
| manifest | basement/manifest/ | 90% | native_manifest_loader.h |
| tools | tools/ (star 脚本) | 90% | star, check_schema_generated.sh |

## Modules 当前状态（配合测试）

| 模块 | 目录 | 用途 |
|------|------|------|
| planning/motion | modules/planning/motion/ | 运动规划 demo |
| planning/navigation | modules/planning/navigation/ | 导航（@deprecated） |
| control/pid | modules/control/pid/ | PID 控制 demo |
| setting/config | modules/setting/config/ | 配置发布 snapshot |
| setting/parameter | modules/setting/parameter/ | 参数服务 demo |
| debug/debug_motion | modules/debug/debug_motion/ | 调试组件（@deprecated） |

**关联 spec**：[basement_index.md](file:///home/zgy/projects/starletto/document/spec/basement_index.md)

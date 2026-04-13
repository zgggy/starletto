# Basement 设计摘要

> 完整设计文档见 `document/spec/basement_design.md`，本文件为项目管理视角的精简版。

## 核心设计原则

1. **Schema-first**：所有类型走 .schema IDL + codegen
2. **Zero-copy**：inproc 指针 / shm loan/view
3. **Native-only**：无 ROS 依赖
4. **Spec-driven**：先写 spec 契约再实现

## 关键接口

| 层 | 核心接口 | 文件 |
|----|----------|------|
| Transport | MessageBus (Reader/Writer/AckWriter) | message_bus.h |
| Transport | ServiceBus (Client/Server/Request/Response) | service_bus.h |
| Runtime | Component (Configure/Start/Stop/Process) | component.h |
| Runtime | ComponentHost (Load/Configure/Start/Stop) | native_component_host.h |
| Schema | SchemaRegistry (Register/Lookup) | schema_registry.h |
| Schema | BinaryCodec (Encode/Decode) | binary_codec.h |

## 数据流关键路径

```
.schema → schema_codegen.py → .s.h → C++ View/Builder
                                         ↓
component.conf → ManifestLoader → ComponentLoadPlan → ComponentHost.Load()
                                                             ↓
                                              auto_configure() → Reader/Writer 注册
                                                                     ↓
                                                              MessageBus.publish()/subscribe()
```

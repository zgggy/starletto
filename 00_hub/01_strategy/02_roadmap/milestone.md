# 里程碑计划与状态

## 已完成里程碑

### M0：Native-only 主路径 ✅

**日期**：2026-04-07 前
**交付**：
- Meson 构建系统 + `./star build/run/install`
- module.conf/component.conf 装配体系
- plugin 动态注册 `star_register_components`
- inproc 指针传递 + shm 零拷贝骨架
- `star list/graph/doctor` 工具链
- 多进程 `--multi-process` fork/exec 骨架

### M1：Schema v1 ✅

**日期**：2026-04-07
**交付**：
- IDL v1（字段 id / enum / oneof / map / repeated / optional / default）
- Codec v1（二进制 wire format tag/len/payload）
- Codegen v1（C++ View/Builder/Descriptor）
- 工具链 v1（`star schema gen/fmt/lint/compat`）
- CI Guard（pre-commit hook）
- Meson 自动集成 codegen

### M2：Transport v1 ✅

**日期**：2026-04-07
**交付**：
- Channel/Reader/Writer 统一接口
- inproc TypedConstPtr 指针传递
- shm loan/view 零拷贝
- routed 自动路由（同组 inproc / 跨组 shm）
- QoS 流控（KEEP_LAST/DROP_NEWEST + per-reader ring buffer）
- CorrelationId 异步调用（P4）

### M3：Runtime v1 ✅

**日期**：2026-04-07 ~ 2026-04-11
**交付**：
- choreography 可表达 + cpu_affinity
- FrameContext 帧语义注入
- SyncTrigger 多 topic 同步触发
- Manifest Schema-typed（B1，ConfValue 死代码清理）
- RollingFileLogger（B2）
- star monitor CLI（F1）
- Latency p99/p999（F2）
- 统一日志格式（F3）
- IClock 抽象（F4）
- Supervisor 增强（F5）
- HealthChecker（F6）

## 进行中 / 待启动里程碑

### B5：测试基础设施 ⬜

**目标**：引入测试框架，为关键路径写单元测试
**验收**：`./star test` 全部通过，覆盖率 > 60%

### M4：系统闭环 ⬜

**目标**：sensor → planning → control 最小数据流闭环
**验收**：多进程场景端到端延迟可测量，record/replay 可复现

## 里程碑依赖关系

```
M0 → M1(Schema) → M2(Transport) → M3(Runtime)
                                    ↓
                              B1(Manifest) ─┐
                              B2(Logger)   ├→ F1-F6(观测性) → B5(测试) → M4(闭环)
```

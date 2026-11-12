# 设计变更记录

| 日期 | 变更内容 | 影响范围 | 决策依据 |
|------|----------|----------|----------|
| 2026-04-07 | 三工程架构边界确定 | 全局 | workplan §0 |
| 2026-04-07 | ConfValue 退出运行时主通路 | manifest/schema | basement_design §1.2 |
| 2026-04-07 | routed 去 env 注入，改 runner→worker 参数传递 | transport/runtime | transport §11 |
| 2026-04-11 | F1-F6 观测性功能新增 | transport/core/runtime | milestone M3 扩展 |
| 2026-09-26 | 组件装配改为纯插件加载：apps 移除静态注册；插件入口签名定为 `star_register_components(ComponentRegistry*)`（原实现把函数指针当 registry 传入，插件加载从未可用） | apps/modules/runtime | arch_review_20260926 §8.6 |
| 2026-09-26 | 分层归位：`schema_value` 移入 `basement/schema`（消除 libraries↔basement 环）；`FixedLayoutCodec` 归入 schema 层（消除 schema→transport 跨层，命名空间随之 `star::transport`→`star::schema`） | basement/libraries | arch_review_20260926 §8.6 |
| 2026-09-26 | schema codegen 迁 `custom_target`：`.schema→.s.h` 为显式构建依赖、输出 build dir、生成物不再入库 | build/schema | arch_review_20260926 §8.9 |
| 2026-09-26 | 错误处理模型统一：运行时状况用状态码返回，编程错误用异常；调度层增加 failed_invocations 计数 | runtime/modules | arch_review_20260926 §8.21 |
| 2026-09-26 | correlation id 引入 DSO 命名空间：多 DSO 唯一性不再依赖「函数内 static 被符号统一」 | transport | arch_review_20260926 §8.19 |
| 2026-09-27 | HealthChecker 按组件自身 `channel_rule` 评估（原实现遍历全局规则，per-component 配置被静默忽略） | runtime/observability | arch_review_20260926 §8.27 |
| 2026-11-11 | shm 头格式 v3→v4：新增 per-slot 代际 gen[]，读侧改为「解码前后双检」；段名带版本号，attach 校验 magic/version/布局 | transport | remediation_plan P0-1 |
| 2026-11-11 | shm 写锁增加 owner 追踪与 owner-death 恢复（有界等待 + kill 探测 + 代际奇偶归一化），段名带版本号 | transport | remediation_plan P0-2 |
| 2026-11-11 | 服务总线随传输方式选择（shm_routed → ShmServiceBus）；服务端口折算进路由计划；parameter 组件改为 commands 声明式 | transport/runtime/modules | remediation_plan P1-1 |
| 2026-11-11 | ShmRoutedMessageBus 实现 RecordableBus（委托 inproc），routed 模式录制恢复 | transport | remediation_plan P1-2 |
| 2026-11-12 | 契约确认：支持多版本 Schema 演进（registry 多版本 + 解码策略 per-channel + record v2）；服务抽象撤销，并入通道（保留轻量 RPC 关联工具） | schema/transport/runtime | contract_decisions |
| 2026-11-12 | 类型身份改为 SchemaTypeId（name+version+hash）：`TypedConstPtr`/`TypedPtr` 携带，写入自动盖章、通道首写采纳；`typeid` 降级为调试信息 | core/schema/transport | contract_decisions 决策一 |
| 2026-11-12 | registry 多版本：key=name+version，同名多版本共存，同 key 不同内容报错；codegen 用 FNV-1a 算内容哈希（字段按 id 排序），lint 校验 hash/未定义类型/map key/version | schema | contract_decisions 决策二/三 阶段二 |
| 2026-11-12 | 逐通道解码策略 unknown/missing/type_mismatch（conf→plan→通道→解码）；同时修复跨进程解码缺陷：shm 默认写二进制 TLV，固定布局帧头 v2 携带 schema 身份并只允许同版本 | schema/transport/runtime | contract_decisions 阶段三；remediation P2-7 |
| 2026-11-12 | 录制格式 v2：墙钟/单调双锚点 + 每条消息携带 schema 身份；回放前 `validate_against()` 校验；v1 文件仍可读 | transport | contract_decisions 阶段四 |
| 2026-11-12 | 服务并入通道：删除 ServiceBus/NativeServiceBus/ShmServiceBus，改为 RpcClient/RpcServer（仅依赖 MessageBus）；payload 信封携带 ok/message | transport/runtime/modules/apps | contract_decisions 决策四 阶段五 |
| 2026-11-12 | monitor 重写：worker 通过 `StatsPublisher` 周期发布 `WorkerStats` 到普通通道 `starletto.stats`；registry 增加「系统发布者」（强制 SHM_WRITE 且不入业务路由表）；monitor 改为普通订阅者，不再自指 | runtime/transport/apps/schema | contract_decisions 附：monitor 自指 |
| 2026-11-12 | shm 读侧线程模型：每 reader 一个轮询线程 → **每通道一个**（多 reader 共享读取、各按 QoS 分发）；reader 析构停线程并 join，剩余 reader 自动重启 poller | transport | remediation_plan P2-3 |
| 2026-11-12 | 日志结构化：文本行加 pid/tid、可选 JSON（`STAR_LOG_FORMAT=json`）、`AsyncLogger` 有界队列（文件日志默认异步，stdout 默认同步）、`module.conf` 的 `logging.level` → plan → host → 调度线程 `ModuleLogScope` | core/runtime/apps | remediation_plan P2-5 |
| 2026-11-12 | 删除 `IClock`（零引用）：仅替换 `now()` 无法驱动 futex/timer/回放节奏，仿真时间需要「统一时钟 + 可唤醒等待 + 回放驱动器」，不是单个接口 | core | remediation_plan P2-6 |
| 2026-11-12 | codegen 拆层：`codegen/{ir,parser,cpp,text}.py`，CLI 只做编排；支持单文件多 root（身份 `<schema>.<root>`、按 root 可达闭包算哈希）；helper 名加 schema 前缀消除 ODR；meson 依赖 codegen 源码 | schema/build | remediation_plan P2-4 |
| 2026-11-12 | codegen 支持跨文件 `import "path"`：直接可见符号表、重名报错、加载器循环检测、`--include-root` 换算 #include；descriptor children 与固定布局引用导入类型 | schema/build | remediation_plan P2-4 |
| 2026-11-12 | schema 支持 `reserved 2, 4;`：禁止复用已删除字段的 id，lint 校验冲突，reserved 集合参与内容哈希；`star check` 增加 codegen 自检步骤（11/11） | schema | remediation_plan P2-2 |
| 2026-11-12 | 新增跨进程 RPC 端到端测试：fork 两进程经 shm 路由完成 request/response（P1-1 遗留的最后验证缺口） | tests/transport | remediation_plan P1-1 遗留 |

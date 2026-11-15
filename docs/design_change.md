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
| 2026-11-14 | 回放写回通道：新增 `MessagePublisher` 与 `star replay --publish/--channel/--repeat/--strict`，录制字节原样发布、schema 身份随行；回放前用 registry 校验身份 | transport/apps | record v2 收口 |
| 2026-11-14 | shm header v5：持有者 pid 表 + attach/析构时清扫已死进程，孤儿段（`_exit`/SIGKILL）自动重建；段名随版本更新，v4 旧段不可达 | transport | remediation_plan P1-4 遗留 |
| 2026-11-14 | 新增 `star new module/component` 脚手架：生成 6 个文件并自动登记父 module.conf/meson，三处命名一次写对；脚手架自检进 CI（12/12） | tools/docs | component_guide 易用性路线 |
| 2026-11-14 | HealthChecker 接线：component.conf 新增 `health` 块（通道频率 + 资源阈值），经 plan 到 host 启动周期评估；严重度变化写日志并发布 `starletto.health`，monitor 展示未健康组件 | runtime/schema/apps | 能力盘点：HealthChecker 零接线 |
| 2026-11-14 | 底层复审修复：① shm 允许空载荷（全 optional 消息的合法编码）；② NativeMessageBus reader 析构等待在途回调（含 cv 丢唤醒）；③ 服务通道升级为系统读写通道，跨进程 RPC 不再依赖路由计划推断客户端组；④ replay speed<=0 防护 | core/transport/runtime/apps | 2026-11-14 复审 |
| 2026-11-14 | shm 段名改用跨工具链稳定的 FNV-1a（替换实现定义的 std::hash），`kShmVersion`→6；golden 单测锁定算法 | core/transport | 2026-11-14 复审 A6 |
| 2026-11-14 | shm 帧头 v7：逐帧携带 schema name/version/hash，`MessageView.schema_type_id` 改为发送方身份；解码策略与诊断可感知对端版本 | transport/runtime/schema | P0.1 实时 wire 身份 |
| 2026-11-14 | schema compat 与版本策略对齐：删字段需 version 递增 + reserved；类型/id 变更保持 breaking；`--allow-breaking` 显式覆盖 | schema/tools | P0.2 |
| 2026-11-14 | 录制路径隔离：`--record-file`/`--record-dir`，多进程强制逐 worker 文件；init 失败组件不进入调度；star check 增加 runtime-smoke（13/13） | runtime/apps | P0.3/P0.4 |
| 2026-11-14 | 插件自注册：register.cc 同时注册模块 schema；apps 删除全部 modules include；replay 工具改用 --plugin；demo 移入模块；新增 app-boundary 门禁与 plugin schema 用例（14/14） | runtime/modules/apps | P1.2 |
| 2026-11-14 | 插件 ABI 契约第一阶段：工具链指纹导出 + doctor --deep 比对（star check 内置）；新增 docs/plugin_abi.md；共享库抽离列为第二阶段 | core/runtime/docs | P1.1 |
| 2026-11-14 | supervisor：ready 握手（STAR_READY_FD）+ on-failure 重启/指数退避/预算；重启录制加 .attemptN 保留现场 | runtime/apps | P1.3 |
| 2026-11-14 | 安全基线：shm 0600 + 属主校验、插件路径限制在模块目录、录制 0600、新增 security_model.md | transport/runtime/docs | P1.4 |
| 2026-11-14 | conf 严格校验：PlanLoader 在 from_value 前跑 SchemaValidator（未知键/缺失/类型），object 容器保持不透明；拼错 readers 不再静默 | runtime/schema | P2.1 |
| 2026-11-14 | 文档/拓扑修正：README 不再宣称 shm 零拷贝（明确跨进程编码+拷贝、View 当前不适用）；资产状态说明；doctor 增加通道两端类型一致性检查 | docs/runtime | P2.2 |
| 2026-11-14 | 随机输入测试 + UBSan：修复固定布局槽位未对齐访问、NaN/越界 double→int64；新增 docs/testing.md 与 CI 模板 | schema/tests/docs | P2.3 |
| 2026-11-14 | app.cc 1140 行按职责拆 5 个 TU（run/inspect/selftest/monitor/common），纯移动、行为不变 | apps | P2.4 |
| 2026-11-14 | 安装面：headers/生成头/schema 工具/pkg-config 安装；新增 oot_plugin_smoke 与 install-smoke 门禁（15/15） | build/docs | P1.1 B1 |
| 2026-11-14 | 共享库 `libstar_runtime` + C ABI v1 试点（同日撤销，见下条） | core/runtime/apps/docs/build | P1.1 B2 |
| 2026-11-14 | 插件 ABI 收敛为终态：删除 C ABI/双入口与 `libstar_runtime` 过渡库；唯一入口 `star_register_components` + 强制工具链指纹；shm 只接受 v7 带身份帧；明确 registry 先于 dlclose 的生命周期 | core/runtime/apps/docs/build | 架构终态 |
| 2026-11-14 | 静态装配落地：模块导出唯一 `star_static_register_*`，生成聚合 TU，新增 `native_runner_static`/`star run --static`/静态冒烟；`static_link=true` 未注册即报错 | build/runtime/apps/star | 部署路径 |
| 2026-11-14 | 删除 `declare_reader/writer`、delivery 组件 API：`auto_configure()` 是唯一端口装配路径；Control 改 `read_latest` | runtime/modules/docs | API 收敛 |
| 2026-11-14 | 删除 C++ schema 定义解析器与 demo，`schema_codegen.py` 单一解析；测试改程序化 descriptor | schema/tests/apps/docs | 单一实现 |
| 2026-11-14 | CI 增加 Coverage job（Meson coverage + 最新 gcovr）与 testing.md 覆盖率/required checks 说明 | .github/docs | 工程门禁 |
| 2026-11-15 | 通道地址统一为绝对路径 `/...`（生产者在前），schema/组件逻辑 ID 用点；服务通道 `<service>/request|response`；list/graph 组件 ID 用 `module.component`；plan 校验拒绝非 `/` 通道 | transport/runtime/apps/modules/docs/tests | 命名终态 |
| 2026-11-15 | trigger 消息随 `process_once(FrameInputs)` 传入：trigger 端口不再建 latest 缓存；普通输入仍 `read_latest`；SyncTrigger 快照三路消息保证同帧 | runtime/transport/modules/tests/docs | 输入语义收敛 |
| 2026-11-15 | 新增 `TypedComponent<Derived, Inputs>`：框架仍只调 `process_once(FrameInputs)`，组件用 `make_inputs()` 把 trigger 消息映射成自有类型化参数；motion/control 迁移 | runtime/modules/tests/docs | 输入 API |
| 2026-11-15 | TypedComponent 终版：TriggerInput<T,"port"> + `const auto&` 参数，去掉 make_inputs/输入 struct/类内 using；motion/control 迁移 | runtime/modules/tests/docs | 输入 API |

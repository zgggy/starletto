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

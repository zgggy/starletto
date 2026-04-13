1. 构建执行总则
执行主体：IDE 内 AI 助手
构建目标：在当前代码仓库根路径下，完成代码库 AI 管理中枢的全量目录、文件、规则体系搭建
强制要求：100% 严格遵循本文件所有规范，不得修改、删减、合并任何目录与文件，不得新增本文件未定义的内容
2. 前置强制命名与路径规范
2.1 根目录规范
根目录固定路径：当前代码仓库根路径/00_hub
根目录必须前置00_前缀，保证 IDE 文件树置顶
2.2 命名铁律
所有目录、文件名优先使用单单词，最多 2 个单词，禁止 3 个及以上单词
仅可使用小写英文字母、数字、下划线，禁止中文、特殊字符、空格
二级目录必须使用数字_前缀，保证 IDE 内固定顺序排序
带日期的文件命名格式：年月日_主题.md，主题为单单词
所有文档类文件统一使用.md格式，脚本文件按对应语言规范使用后缀
3. 核心入口文件规范
3.1 INIT.md
文件路径：00_hub/INIT.md
必须按以下顺序创建内容模块：
代码库核心定位
执行主体核心职责与权责边界
目录架构总览与检索优先级
输出内容强制规范
目录体系使用规则
紧急场景执行流程
3.2 index.md
文件路径：00_hub/04_assets/index.md
必须按以下顺序创建内容模块：
完整目录树总览
战略决策文件索引
全量任务索引
研发设计文件索引
发布交付文件索引
共享资产索引
4. 完整目录与文件构建清单
plaintext
00_hub
├─ INIT.md
│
├─ 01_strategy
│  ├─ 01_vision
│  │  ├─ position.md
│  │  ├─ rules.md
│  │  └─ goals.md
│  ├─ 02_roadmap
│  │  ├─ version.md
│  │  ├─ milestone.md
│  │  └─ release.md
│  └─ 03_decisions
│     ├─ index.md
│     ├─ 2026xxxx_tech.md
│     ├─ 2026xxxx_arch.md
│     └─ 2026xxxx_priority.md
│
├─ 02_tasks
│  ├─ 01_pending
│  │  ├─ backlog.md
│  │  ├─ feature.md
│  │  ├─ bug.md
│  │  ├─ optimize.md
│  │  └─ techdebt.md
│  ├─ 02_active
│  │  ├─ sprint.md
│  │  ├─ tracking.md
│  │  └─ daily.md
│  └─ 03_archived
│     ├─ index.md
│     ├─ 2026xxxx_feature.md
│     ├─ 2026xxxx_bug.md
│     ├─ 2026xxxx_optimize.md
│     └─ 2026xxxx_techdebt.md
│
├─ 03_dev
│  ├─ 01_design
│  │  ├─ arch.md
│  │  ├─ stack.md
│  │  ├─ modules.md
│  │  ├─ 02_modules
│  │  │  ├─ index.md
│  │  │  ├─ [module]_design.md
│  │  │  └─ [module]_flow.md
│  │  ├─ 03_db
│  │  │  ├─ table.md
│  │  │  ├─ db_index.md
│  │  │  ├─ api.md
│  │  │  └─ api_change.md
│  │  └─ design_change.md
│  ├─ 02_specs
│  │  ├─ code.md
│  │  ├─ git.md
│  │  ├─ review.md
│  │  ├─ env.md
│  │  └─ deps.md
│  ├─ 03_test
│  │  ├─ cases.md
│  │  ├─ report.md
│  │  ├─ bugfix.md
│  │  └─ perf.md
│  └─ 04_release
│     ├─ index.md
│     ├─ [version]_record.md
│     ├─ deploy.md
│     ├─ changelog.md
│     └─ rollback.md
│
└─ 04_assets
   ├─ index.md
   └─ 02_shared
      ├─ 01_templates
      │  ├─ prd.md
      │  ├─ design.md
      │  ├─ task.md
      │  ├─ decision.md
      │  ├─ sprint.md
      │  └─ release.md
      ├─ 02_scripts
      │  ├─ deploy.sh
      │  ├─ git.py
      │  ├─ init.sh
      │  └─ task_sync.py
      └─ 03_snippets
         ├─ components.md
         ├─ utils.md
         └─ tools.md
5. 执行主体标准运行 SOP
每次对话触发，必须严格按以下顺序执行，禁止跳步：
读取00_hub/INIT.md，完成规则对齐与权责校验
读取00_hub/04_assets/index.md，定位目标关联文件
读取关联文件，获取完整上下文信息
完成对应需求任务执行
同步更新对应目录下的目标文件，同步刷新00_hub/04_assets/index.md全局索引
输出执行结果与更新的文件路径
6. 触发式自动更新规则
以下场景触发时，必须自动完成对应文件的更新，无需额外指令：
表格
触发场景	必须更新的文件路径
新增需求 / BUG / 架构优化 / 技术债务任务	02_tasks/01_pending / 对应分类文件、02_tasks/01_pending/backlog.md、04_assets/index.md
任务 / 迭代启动确认	02_tasks/01_pending / 对应文件、02_tasks/02_active / 对应文件、02_tasks/02_active/sprint.md、04_assets/index.md
任务完成 / 问题解决确认	02_tasks/02_active / 对应文件、02_tasks/03_archived / 对应归档文件、02_tasks/03_archived/index.md、01_strategy/02_roadmap/milestone.md、04_assets/index.md
架构 / 技术选型 / 优先级决策确认	01_strategy/03_decisions / 对应文件、03_dev/01_design / 对应设计文件、04_assets/index.md
版本发布 / 部署完成	03_dev/04_release / 对应发布文件、03_dev/04_release/changelog.md、01_strategy/02_roadmap/version.md、04_assets/index.md
模板 / 脚本 / 组件新增 / 修改	04_assets/02_shared / 对应文件、04_assets/index.md
7. 构建完成校验清单
构建完成后，必须 100% 通过以下所有校验项，方可确认构建完成：
 已创建根目录00_hub，路径为当前代码仓库根路径 / 00_hub
 已创建00_hub/INIT.md，包含本文件规定的所有内容模块
 已完整创建 4 个一级目录，命名完全符合本文件规范
 已完整创建所有二级、三级目录，层级与命名完全符合本文件规范
 已完整创建本文件清单内的所有文件，无遗漏、无合并、无删减
 已创建00_hub/04_assets/index.md，包含本文件规定的所有内容模块
 所有模板文件已创建，包含对应场景的必填字段
 所有路径与命名完全符合本文件的命名铁律
 已完成目录结构与全局索引的一致性校验，索引无缺失项
# 全局索引

## 完整目录树总览

```
00_hub/
├─ INIT.md
│
├─ 01_strategy/
│  ├─ 01_vision/
│  │  ├─ position.md        # 项目定位与市场位置
│  │  ├─ rules.md            # 开发铁律与约束
│  │  ├─ goals.md            # 阶段目标与成功标准（含代码资产量化）
│  │  └─ engineering_analysis.md  # ⭐ 工程现状深度分析（数据驱动）
│  ├─ 02_roadmap/
│  │  ├─ version.md          # 版本定义与语义
│  │  ├─ milestone.md        # 里程碑计划与状态
│  │  └─ release.md          # 发布节奏策略
│  └─ 03_decisions/
│     ├─ index.md            # 决策索引
│     ├─ 20260411_tech.md    # 技术选型决策
│     ├─ 20260411_arch.md    # 架构决策
│     └─ 20260411_priority.md# 优先级决策
│
├─ 02_tasks/
│  ├─ 01_pending/
│  │  ├─ backlog.md         # 全量待办 backlog
│  │  ├─ feature.md          # 待办功能需求
│  │  ├─ bug.md              # 待办缺陷
│  │  ├─ optimize.md         # 待办优化项
│  │  └─ techdebt.md         # 技术债务清单
│  ├─ 02_active/
│  │  ├─ sprint.md           # 当前 sprint 计划
│  │  ├─ tracking.md         # 任务追踪看板
│  │  └─ daily.md            # 每日站会记录
│  └─ 03_archived/
│     ├─ index.md            # 归档索引
│     ├─ 20260411_feature.md # 已归档功能
│     ├─ 20260411_bug.md     # 已归档缺陷
│     ├─ 20260411_optimize.md# 已归档优化
│     └─ 20260411_techdebt.md# 已归档技术债务
│
├─ 03_dev/
│  ├─ 01_design/
│  │  ├─ arch.md             # 系统架构设计
│  │  ├─ stack.md            # 技术栈选型
│  │  ├─ modules.md          # 模块划分
│  │  ├─ 02_modules/
│  │  │  ├─ index.md         # 模块设计索引
│  │  │  ├─ basement_design.md
│  │  │  └─ basement_flow.md
│  │  ├─ 03_db/              # （预留，当前无数据库）
│  │  └─ design_change.md    # 设计变更记录
│  ├─ 02_specs/
│  │  ├─ code.md             # 编码规范
│  │  ├─ git.md              # Git 工作流
│  │  ├─ review.md           # Code Review 规范
│  │  ├─ env.md              # 环境规范
│  │  └─ deps.md             # 依赖管理
│  ├─ 03_test/
│  │  ├─ cases.md            # 测试用例
│  │  ├─ report.md           # 测试报告
│  │  ├─ bugfix.md           # 缺陷修复记录
│  │  └─ perf.md             # 性能测试
│  └─ 04_release/
│     ├─ index.md            # 发布索引
│     ├─ v0.1_record.md      # 版本发布记录
│     ├─ deploy.md           # 部署指南
│     ├─ changelog.md        # 变更日志
│     └─ rollback.md         # 回滚方案
│
└─ 04_assets/
   ├─ index.md               # 资产索引
   └─ 02_shared/
      ├─ 01_templates/       # 文档模板
      ├─ 02_scripts/         # 自动化脚本
      └─ 03_snippets/        # 代码片段库
```

## 战略决策文件索引

| 文件 | 说明 | 状态 |
|------|------|------|
| [position.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/01_vision/position.md) | 项目定位 | ✅ 已创建 |
| [rules.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/01_vision/rules.md) | 开发铁律 | ✅ 已创建 |
| [goals.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/01_vision/goals.md) | 阶段目标（含代码量化） | ✅ 已优化 |
| [engineering_analysis.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/01_vision/engineering_analysis.md) | ⭐ **工程现状深度分析** | ✅ 新建 |
| [version.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/02_roadmap/version.md) | 版本定义 | ✅ 已创建 |
| [milestone.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/02_roadmap/milestone.md) | 里程碑 | ✅ 已创建 |
| [release.md](file:///home/zgy/projects/starletto/00_hub/01_strategy/02_roadmap/release.md) | 发布节奏 | ✅ 已创建 |

## 全量任务索引

| 分类 | 文件 | 待办 | 进行中 | 已完成 |
|------|------|------|--------|--------|
| 功能 | [feature.md](file:///home/zgy/projects/starletto/00_hub/02_tasks/01_pending/feature.md) | — | — | F1-F6 ✅ |
| 缺陷 | [bug.md](file:///home/zgy/projects/starletto/00_hub/02_tasks/01_pending/bug.md) | 0 | 0 | 0 |
| 优化 | [optimize.md](file:///home/zgy/projects/starletto/00_hub/02_tasks/01_pending/optimize.md) | — | — | — |
| 技术债务 | [techdebt.md](file:///home/zgy/projects/starletto/00_hub/02_tasks/01_pending/techdebt.md) | — | — | — |

## 研发设计文件索引

| 文件 | 说明 | 关联 spec |
|------|------|-----------|
| [arch.md](file:///home/zgy/projects/starletto/00_hub/03_dev/01_design/arch.md) | 系统架构 | system_vision.md |
| [stack.md](file:///home/zgy/projects/starletto/00_hub/03_dev/01_design/stack.md) | 技术栈 | — |
| [modules.md](file:///home/zgy/projects/starletto/00_hub/03_dev/01_design/modules.md) | 模块划分 | basement_index.md |
| [code.md](file:///home/zgy/projects/starletto/00_hub/03_dev/02_specs/code.md) | 编码规范 | project_rules.md |

## 发布交付文件索引

| 版本 | 日期 | 关键交付 |
|------|------|----------|
| v0.1 | 2026-04-07 | M0-M3 基础底座 + P0-P5 核心能力 |
| v0.2 | 2026-04-11 | B1-B2 工程化 + F1-F6 观测性补全 |

## 共享资产索引

| 类别 | 文件 | 用途 |
|------|------|------|
| 模板 | `04_assets/02_shared/01_templates/` | prd/design/task/decision/sprint/release |
| 脚本 | `04_assets/02_shared/02_scripts/` | deploy/git/init/task_sync |
| 片段 | `04_assets/02_shared/03_snippets/` | components/utils/tools |

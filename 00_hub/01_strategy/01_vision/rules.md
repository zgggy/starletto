# 开发铁律与约束

## 1. 架构约束（不可违反）

| 编号 | 规则 | 来源 |
|------|------|------|
| A1 | 单进程 payload 必须指针传递，不得序列化 | basement_design §2.2 |
| A2 | 跨进程 payload 必须 shm 零拷贝，必须具备 loan/view 路径 | basement_design §2.2 |
| A3 | Native-only：主路径不考虑 ROS，任何 bridge 必须独立工程 | basement_design §2.2 |
| A4 | Schema 统一性：配置/参数/消息/服务请求响应/事件共享同一 schema 体系 | basement_design §2.2 |
| A5 | 三工程边界：basement/libraries/modules 各自独立 spec | workplan §0 |

## 2. 开发流程约束（Spec-first）

| 编号 | 规则 | 说明 |
|------|------|------|
| F1 | 先定位 Spec → 写契约再实现 | project_rules Step A-B |
| F2 | 单进程指针传递 / 跨进程 shm 零拷贝 | project_rules Step C |
| F3 | 每个里程碑必须有可执行验收命令 | project_rules Step D |
| F4 | 代码变更提交前保证 spec 同步更新 | project_rules Step E |

## 3. 代码规范约束

| 编号 | 规则 | 说明 |
|------|------|------|
| C1 | 所有代码尽量放在 .h 文件中 | user_rules §1.1 |
| C2 | 默认不添加代码注释 | user_rules §1.2 |
| C3 | PascalCase 类名 / snake_case 变量 / UPPER_SNAKE_CASE 常量 | user_rules §2.1 |
| C4 | 返回类型后置（void/bool 除外） | user_rules §3.1 |
| C5 | 严禁硬编码（数值/字符串/枚举） | user_rules §3.5 |
| C6 | 头文件保护使用传统 `#ifndef` 风格 | user_rules §4.1 |
| C7 | 缩进 4 空格，不使用 Tab | user_rules §4.4 |

## 4. 工程纪律

| 编号 | 规则 | 说明 |
|------|------|------|
| E1 | 不自动打开开发服务器 | user_rules §9 |
| E2 | 不自动编译 | user_rules §9 |
| E3 | 优先使用项目现有构建系统（Meson） | user_rules §9 |
| E4 | CI Guard：pre-commit hook 确保 .s.h 与 codegen 输出一致 | tools/check_schema_generated.sh |

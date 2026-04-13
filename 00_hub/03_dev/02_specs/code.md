# 编码规范

> 详细规范见 `.trae/rules/project_rules.md`（项目规则）和用户个人规范。本文件为项目管理视角摘要。

## 核心规则速查

| 规则 | 值 | 来源 |
|------|-----|------|
| 代码存放 | .h 文件为主 | user_rules §1.1 |
| 注释 | 默认不添加 | user_rules §1.2 |
| 类命名 | PascalCase | user_rules §2.1 |
| 变量命名 | snake_case | user_rules §2.1 |
| 私有成员 | snake_case_ | user_rules §2.1 |
| 常量/宏 | UPPER_SNAKE_CASE | user_rules §2.1 |
| 返回类型 | 后置语法（void/bool 除外） | user_rules §3.1 |
| 智能指针 | unique_ptr/shared_ptr/weak_ptr | user_rules §3.2 |
| 类型转换 | static/dynamic/const/reinterpret_cast | user_rules §3.4 |
| 空指针 | nullptr | user_rules §3.4 |
| 硬编码 | 严禁 | user_rules §3.5 |
| 头文件保护 | #ifndef 风格 | user_rules §4.1 |
| 缩进 | 4 空格 | user_rules §4.4 |

## 函数命名分类

| 类型 | 风格 | 示例 |
|------|------|------|
| 查询/取值/设置 | snake_case | `name()`, `get_distance()`, `set_status()` |
| 操作/执行 | CamelCase | `Enter()`, `Process()`, `OptimizePath()` |

## 日志宏

```cpp
logD << "debug message";
logI << "info message";
logW << "warning message";
logE << "error message";
```

**完整规范文件**：`.trae/rules/project_rules.md`

# 缺陷修复记录

## 已修复缺陷

| ID | 标题 | 日期 | 根因 | 修复方案 | 状态 |
|----|------|------|------|----------|------|
| BUG-001 | monitor() 缺少 options 参数 | 2026-04-11 | 方法签名遗漏 RunOptions 参数 | 添加 const RunOptions& 参数 + 调用处传参 | ✅ |
| BUG-002 | std::ceil 缺少 <cmath> | 2026-04-11 | compute_percentile 使用 std::ceil 但未 include cmath | 添加 #include <cmath> | ✅ |
| BUG-003 | ShmMessageBus 抽象类错误 | 2026-04-11 | declared_topics() 新增纯虚接口但 ShmMessageBus 未实现 | 添加 declared_topics() override | ✅ |
| BUG-004 | RollingFileLogger 未集成 | 2026-04-11 | 创建了文件但未在 runtime_factory.h 中使用 | 在 STAR_LOG_FILE 分支使用 RollingFileLogger | ✅ |
| BUG-005 | CI guard header guard 不匹配 | 2026-04-11 | codegen 基于绝对路径生成 guard，临时文件 guard 不同 | normalize_guard() sed 替换 | ✅ |
| BUG-006 | pre-commit hook 路径解析错误 | 2026-04-11 | $0 在 .git/hooks/ 下导致 $(dirname $0)/.. 解析到 .git/ | 改为 $(dirname $0)/../.. | ✅ |
| BUG-007 | CorrelationId 命名空间缺失 | 2026-04-11 | runtime_factory.h 中使用 CorrelationId 未加 star::transport:: | 使用全限定名 | ✅ |
| BUG-008 | unordered_map<CorrelationId> 无 hash | 2026-04-11 | 自定义类型需要 std::hash 特化 | 在 service_bus.h 添加特化 | ✅ |

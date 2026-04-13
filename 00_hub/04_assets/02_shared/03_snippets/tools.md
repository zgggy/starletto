# 工具命令片段

## star 命令速查

```bash
# 构建
./star build

# 运行（单进程）
./star run --frames=100 --sleep-ms=10 modules/planning/motion/component.conf

# 运行（多进程）
./star run --multi-process planning/module.conf

# 监控
./star monitor --transport=inproc modules/planning/motion/component.conf

# 列表
./star list planning/module.conf

# 图形
./star graph=component-dot planning/module.conf
./star graph=module-dot planning/module.conf

# 诊断
./star doctor planning/module.conf
./star doctor --deep planning/module.conf

# Schema 工具
./star schema gen              # 重新 codegen
./star schema fmt              # 格式化 .schema
./star schema lint             # 检查 .schema
./star schema compat           # 兼容性检查

# 安装
./star install
```

## 环境变量组合

```bash
# 完整调试运行
STAR_LOG_FILE=/tmp/star.log \
STAR_TRANSPORT_STATS=1 \
./star run --frames=100 motion.conf

# 多进程 + 日志分离
./star run --multi-process --log-dir=/tmp/star_logs planning/module.conf
```

## CI Guard 手动执行

```bash
# 检查所有 .s.h 是否与 codegen 输出一致
bash tools/check_schema_generated.sh

# 强制重新生成
./star schema gen && ./star build
```

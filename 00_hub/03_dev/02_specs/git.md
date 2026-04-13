# Git 工作流

## 分支策略

当前采用 **单主分支** 模式（main only）。

## 提交规范

- 每次功能完成后再 commit（不要求原子提交）
- commit message 格式：简短描述变更内容
- 不强制使用 Conventional Commits

## CI Guard

pre-commit hook 自动运行：

```bash
bash tools/check_schema_generated.sh
```

确保所有 .s.h 文件与 codegen 输出一致。

## 关键命令

| 操作 | 命令 |
|------|------|
| 构建 | `./star build` |
| 运行 | `./star run --frames=N --sleep-ms=10 <conf>` |
| 多进程 | `./star run --multi-process <module.conf>` |
| 监控 | `./star monitor --transport=inproc <conf>` |
| 列表 | `./star list <module.conf>` |
| 图形 | `./star graph=component-dot <module.conf>` |
| 诊断 | `./star doctor [--deep] <module.conf>` |
| Schema | `./star schema gen/fmt/lint/compat` |
| 安装 | `./star install` |

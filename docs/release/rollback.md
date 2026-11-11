# 回滚方案

## 回滚策略

当前使用 Git 进行版本控制，回滚通过 git 操作完成。

### 快速回滚（未 push）

```bash
git checkout HEAD~1 -- .
./star build   # 验证回滚后可编译
```

### 版本回滚（已 tag）

```bash
git checkout v0.1    # 回滚到 v0.1
./star build          # 验证
```

### 特定文件回滚

```bash
git checkout HEAD~1 -- <file_path>
```

## 回滚检查清单

- [ ] `./star build` 零错误
- [ ] 单进程端到端测试通过
- [ ] 多进程编排正常（如使用）
- [ ] `bash tools/check_schema_generated.sh` 通过
- [ ] spec 文档与代码版本一致

## 数据安全

- 配置文件（.conf）在源码树中，不受回滚影响
- 录制文件（.bin）在 /tmp 或 STAR_REC_FILE 指定路径，不受回滚影响
- 日志文件在 STAR_LOG_FILE 指定路径，不受回滚影响

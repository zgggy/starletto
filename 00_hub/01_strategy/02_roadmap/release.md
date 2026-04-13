# 发布节奏策略

## 发布策略

当前阶段采用 **持续交付** 模式，不做固定周期发布。

### 触发条件

以下任一条件满足时考虑打版本：

1. 完成一个完整里程碑（M0/M1/M2/M3/M4）
2. 完成一组功能批次（如 F1-F6、B1-B2）
3. 重大架构决策落地

### 发布流程

1. 更新 `03_dev/04_release/changelog.md`
2. 更新 `03_dev/04_release/v{version}_record.md`
3. 更新 `01_strategy/02_roadmap/version.md`
4. 运行全量验证：
   - `./star build` 零错误
   - `bash tools/check_schema_generated.sh` 通过
   - 单进程 + 多进程端到端测试通过
   - `STAR_TRANSPORT_STATS=1 ./star run ...` 统计输出正常

### 发布物

- source tree（git tag: `v{version}`）
- build artifacts（`build/` 目录）
- spec 文档快照（`document/spec/`）

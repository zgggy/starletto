# 任务模板

```markdown
# [任务 ID] [任务标题]

## 元信息

| 属性 | 值 |
|------|-----|
| 类型 | feature / bug / optimize / techdebt |
| 优先级 | P0 / P1 / P2 / P3 |
| 状态 | pending / in_progress / done |
| 关联 spec | [spec 文件路径] |
| 关联决策 | [decision 文件路径] |

## 描述

（详细描述任务内容）

## 实现计划

1. Step 1: ...
2. Step 2: ...
3. Step 3: ...

## 验收标准

- [ ] `./star build` 零错误
- [ ] 端到端验证通过
- [ ] spec 文档已同步更新
- [ ] CI guard 通过

## 变更文件清单

- [ ] 新增: `path/to/file.h`
- [ ] 修改: `path/to/file.h`
- [ ] 删除: `path/to/file.h`

## 备注

（其他需要记录的信息）
```

# 环境规范

## 开发环境

| 项目 | 要求 |
|------|------|
| OS | Linux (Ubuntu 24.04+) |
| 编译器 | GCC 13+ 或 Clang 16+ |
| Python | 3.8+ (用于 codegen) |
| Meson | 0.60+ |
| Ninja | 1.10+ |

## 环境变量

| 变量 | 用途 | 默认值 |
|------|------|--------|
| STAR_LOG_FILE | 日志文件路径（触发 RollingFileLogger） | 未设置 |
| STAR_TRANSPORT_STATS | 启用 transport 统计输出 | 未设置 |
| STAR_LIB_PATH | 动态库搜索路径 | 未设置 |
| STAR_REC_FILE | 录制文件路径 | /tmp/starletto_record.bin |

## 构建目录

```
starletto/
├── build/              # Meson 构建输出
│   ├── gen/            # codegen 产物 (.s.h)
│   └── artifacts/      # install 输出
├── basement/           # 底座源码
├── libraries/          # 算法库
├── modules/            # 业务模块
├── apps/               # 入口程序
├── tools/              # 工具脚本
├── document/           # 文档（spec + build.md）
└── 00_hub/             # 项目管理中枢 ← 本目录
```

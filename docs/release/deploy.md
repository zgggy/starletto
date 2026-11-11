# 部署指南

## 当前部署方式

### 开发模式（推荐）

```bash
cd /home/zgy/projects/starletto
./star build
./star run --frames=100 --sleep-ms=10 <component_or_module_conf>
```

### Install 模式

```bash
./star install   # 输出到 build/artifacts/
# artifacts/ 目录可独立运行，包含 bin/lib/modules/config
```

### 多进程模式

```bash
./star run --multi-process <module.conf>
# 按 process_group 自动拆分进程
# 日志按 group 分离
```

## 环境变量参考

| 变量 | 说明 | 示例 |
|------|------|------|
| STAR_LOG_FILE | 启用 RollingFileLogger | `/var/log/star/star.log` |
| STAR_TRANSPORT_STATS | 启用 transport 统计输出 | `1` |
| STAR_LIB_PATH | 动态库搜索路径 | `/opt/star/lib:/usr/local/lib` |

## 目录结构（install 后）

```
artifacts/
├── bin/
│   └── native_runner
├── lib/
│   ├── libconfig.so
│   ├── libmotion.so
│   ├── libparameter.so
│   └── ...
├── modules/
│   ├── planning/
│   │   └── motion/component.conf
│   ├── control/
│   │   └── pid/component.conf
│   └── setting/
│       ├── config/
│       └── parameter/
└── config/
    └── *.conf
```

# modules —— 业务组件

每个组件是一个独立可装配单元：目录 `modules/<module>/<component>/`，通过
`module.conf` 的 `components` 列表登记。组件代码只依赖 `basement/` 和
`modules/setting/schema/`（公共消息定义），不反向依赖 apps。

## 命名：路径用 `/`，逻辑名用 `.`

| 对象 | 写法 | 示例 |
|---|---|---|
| 文件系统 / CLI 目标 | `/` | `modules/planning/motion`、`star run planning/motion` |
| 组件逻辑 ID | `.` | `planning.motion`（日志/graph/诊断） |
| 通道地址 | `/`（绝对路径） | `/perception/prediction`、`/localization/pose`、`/control/chassis` |
| C++ 类型 | `::` | `star::planning::Motion` |

通道地址是数据存放位置（生产者在前）；schema 类型名才用点。

## 目录约定

```
modules/<module>/
├── module.conf              # 模块名 + components 列表
├── meson.build
└── <component>/
    ├── component.conf       # 端口/trigger/类名/库名
    ├── <component>_component.h
    ├── <component>_processor.h
    ├── register.cc          # 插件入口 + 唯一静态入口
    ├── meson.build          # .so + .a + 静态注册数组
    └── README.md
```

## 装配方式

- 调试：`component_library` 指向 `.so`，`native_runner` dlopen；
- 部署：`native_runner_static` 链接 `star_static_register_<module>_<component>`，
  `static_link=true` 时不再 dlopen；
- 两种方式共用同一份 `component.conf`/plan/组件代码。

新增组件用 `./star new component <module>/<name>`，脚手架会一次写对
`component_library`、`component_class`、插件入口、静态库数组和父级接线。

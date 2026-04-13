# Basement 数据流

## 启动流程

```
./star run motion.conf
    │
    ├── parse_args() → RunOptions
    │
    ├── App::run(options, paths)
    │   ├── RuntimeFactory::create_native_context(transport_name)
    │   │   └── 创建 NativeMessageBus / ShmRoutedMessageBus
    │   │
    │   ├── register_generated_schemas()  ← meson run_command 预生成
    │   ├── register_business_schemas()   ← schema_bootstrap.h
    │   │
    │   ├── build_component_load_plan(conf) → ManifestLoader → ComponentManifest
    │   │   └── enrich_component_manifest() → module.conf 解析
    │   │
    │   ├── host.load(plan) → dlopen(lib) → star_register_components()
    │   │
    │   ├── host.configure() → auto_configure(plan)
    │   │   └── 对每个 EndpointPlan: message_bus.create_writer/create_reader
    │   │       └── QoS 从 plan.qos_depth / qos_drop_policy 读取
    │   │
    │   ├── host.start() → Scheduler 启动 trigger 线程
    │   │   └── timer trigger → process_once() 循环
    │   │       └── writer.write(msg) → publish_internal() → reader callback
    │   │           └── 记录 latency_samples → compute p99/p999
    │   │
    │   └── host.wait_for_frames() → 超时检测
    │
    └── maybe_dump_transport_stats()  ← STAR_TRANSPORT_STATS=1 时输出
```

## 多进程流程

```
./star run --multi-process planning/module.conf
    │
    ├── parse_module_conf() → groups[{name, components}]
    │
    ├── build_route_plan(topic_producers, topic_consumers)
    │   └── 跨组 topic 自动标记为 shm 路由
    │
    ├── for each group:
    │   └── orchestrator.launch_worker(config)
    │       └── fork() + execv(native_runner --worker --route-plan=...)
    │           └── worker 进程独立运行（同上单进程流程）
    │
    └── orchestrator.wait_all(pids) → [Supervisor] 日志 + exit code 聚合
```

## Codegen 流程（构建时）

```
meson setup
    └── run_command(python3 schema_codegen.py --schema-dir X --output-dir Y)
        ├── for each *.schema:
        │   ├── parse IDL → AST (SchemaAST)
        │   ├── validate (SchemaValidator)
        │   └── generate .s.h (C++ View/Builder/Descriptor/Codec glue)
        └── 输出 N 个 .s.h 文件到 build/gen/

ninja compile
    └── #include "gen/*.s.h"  ← 使用 codegen 产物
```

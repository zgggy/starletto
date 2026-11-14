# 观测 schema：worker 周期发布到 starletto.stats 普通通道，monitor 作为普通订阅者读取
schema_gen_basement_schema_builtin_observability = custom_target(
    'gen_basement_schema_builtin_observability',
    input: files('worker_stats.schema', 'worker_health.schema'),
    output: ['worker_stats.s.h', 'worker_health.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema-dir', meson.current_source_dir(),
        '--output-dir', meson.current_build_dir(),
        '--include-root', meson.project_source_root(),
    ],
    depend_files: codegen_sources,
    install: true,
    install_dir: get_option('includedir') / 'starletto' / 'basement' / 'schema' / 'builtin' / 'observability',
    build_by_default: true,
)

# 由 schema_codegen.py 生成 .s.h（custom_target 声明 .schema -> .s.h 依赖，
# 输出到 build dir；改 .schema 会触发重新生成，杜绝源/生成物漂移）
schema_gen_basement_schema_builtin_runtime = custom_target(
    'gen_basement_schema_builtin_runtime',
    input: files('component_load_plan.schema', 'process_plan.schema'),
    output: ['component_load_plan.s.h', 'process_plan.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema-dir', meson.current_source_dir(),
        '--output-dir', meson.current_build_dir(),
    ],
    build_by_default: true,
)

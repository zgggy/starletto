# 由 schema_codegen.py 生成 .s.h（custom_target 声明 .schema -> .s.h 依赖，
# 输出到 build dir；改 .schema 会触发重新生成，杜绝源/生成物漂移）
schema_gen_modules_setting_schema_control = custom_target(
    'gen_modules_setting_schema_control',
    input: files('command.schema'),
    output: ['command.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema-dir', meson.current_source_dir(),
        '--output-dir', meson.current_build_dir(),
    ],
    build_by_default: true,
)

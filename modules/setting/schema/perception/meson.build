# 由 schema_codegen.py 生成 .s.h（custom_target 声明 .schema -> .s.h 依赖）
schema_gen_modules_setting_schema_perception = custom_target(
    'gen_modules_setting_schema_perception',
    input: files('prediction.schema'),
    output: ['prediction.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema-dir', meson.current_source_dir(),
        '--output-dir', meson.current_build_dir(),
    ],
    depend_files: codegen_sources,
    build_by_default: true,
)

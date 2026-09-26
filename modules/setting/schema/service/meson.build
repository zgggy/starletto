# 由 schema_codegen.py 生成 .s.h（custom_target 声明 .schema -> .s.h 依赖，
# 输出到 build dir；改 .schema 会触发重新生成，杜绝源/生成物漂移）
schema_gen_modules_setting_schema_service = custom_target(
    'gen_modules_setting_schema_service',
    input: files('parameter_get_request.schema', 'parameter_get_response.schema', 'parameter_list_request.schema', 'parameter_list_response.schema', 'parameter_set_request.schema', 'parameter_set_response.schema'),
    output: ['parameter_get_request.s.h', 'parameter_get_response.s.h', 'parameter_list_request.s.h', 'parameter_list_response.s.h', 'parameter_set_request.s.h', 'parameter_set_response.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema-dir', meson.current_source_dir(),
        '--output-dir', meson.current_build_dir(),
    ],
    build_by_default: true,
)

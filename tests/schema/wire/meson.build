# P0.1：同名 schema 的 v1/v2 同时生成，用于跨版本 wire 身份测试
wire_schemas_gen = custom_target(
    'gen_test_wire_schemas',
    input: files('v1.schema', 'v2.schema'),
    output: ['v1.s.h', 'v2.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema-dir', meson.current_source_dir(),
        '--output-dir', meson.current_build_dir(),
        '--include-root', meson.project_source_root(),
    ],
    depend_files: codegen_sources,
    build_by_default: true,
)

# P2-4：跨文件 import 测试（base.schema 的类型被 uses.schema 引用）。
# include-root = 源码根：生成头里的 #include 路径相对于 build 根镜像，
# 与编译器的 -I<build root> 对齐。
import_schemas_gen = custom_target(
    'gen_test_import_schemas',
    input: files('base.schema', 'uses.schema'),
    output: ['base.s.h', 'uses.s.h'],
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

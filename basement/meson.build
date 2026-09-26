starletto_inc = include_directories(
    '.',
    '..',
)

cpp = meson.get_compiler('cpp')
dl_dep = cpp.find_library('dl', required: false)
rt_dep = cpp.find_library('rt', required: false)

codegen_py = find_program('python3', required: true)
codegen_script = files('schema/schema_codegen.py')

base_dir = meson.current_source_dir()

# codegen：.schema -> .s.h 由 custom_target 处理（见各 schema 目录的 meson.build）
subdir('schema')

star_core_dep = declare_dependency(
    include_directories: starletto_inc,
)

schema_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [star_core_dep],
    sources: [
        schema_gen_basement_schema_builtin_manifest,
        schema_gen_basement_schema_builtin_runtime,
    ],
)

runtime_api_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [star_core_dep, schema_dep],
)

transport_api_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [star_core_dep, schema_dep, runtime_api_dep],
)

manifest_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [star_core_dep, schema_dep],
)

manifest_native_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [manifest_dep],
)

transport_native_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [transport_api_dep, rt_dep],
)

runtime_native_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [runtime_api_dep, transport_native_dep, manifest_native_dep, dl_dep],
)

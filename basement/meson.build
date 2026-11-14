starletto_inc = include_directories(
    '.',
    '..',
)

cpp = meson.get_compiler('cpp')
dl_dep = cpp.find_library('dl', required: false)
rt_dep = cpp.find_library('rt', required: false)

codegen_py = find_program('python3', required: true)
codegen_script = files('schema/schema_codegen.py')
# CLI 之外的实现模块：改它们也必须触发重新生成，否则生成物会静默过期
codegen_sources = files(
    'schema/codegen/__init__.py',
    'schema/codegen/ir.py',
    'schema/codegen/parser.py',
    'schema/codegen/cpp.py',
    'schema/codegen/text.py',
)

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
        schema_gen_basement_schema_builtin_observability,
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

# macOS 默认把 dylib 的 install_name 写成绝对路径，安装树不可搬迁；
# 改用 @rpath，配合可执行文件的 @loader_path/../lib 才能真正 relocatable。
star_runtime_link_args = host_machine.system() == 'darwin' ? ['-Wl,-install_name,@rpath/libstar_runtime.0.dylib'] : []

# 共享运行时库：目前承载 C ABI 插件适配器与 ABI 探测入口。
# core/schema/transport 仍是 header-only；这里是「非模板实现下沉」的第一块，
# 后续可以把更多实现迁进来而不改变插件 ABI。
star_runtime_lib = both_libraries(
    'star_runtime',
    'runtime/component/c_abi_adapter.cc',
    include_directories: starletto_inc,
    dependencies: [schema_dep, transport_native_dep],
    version: meson.project_version(),
    soversion: '0',
    link_args: star_runtime_link_args,
    install: true,
)

# 宿主可执行文件静态链接适配层：安装后不依赖 dylib 的绝对 install_name，
# DESTDIR/容器/relocatable 部署都能直接运行。
star_runtime_dep = declare_dependency(
    include_directories: starletto_inc,
    link_with: star_runtime_lib.get_static_lib(),
    dependencies: [transport_native_dep],
)

# 显式链接共享库的依赖：给需要验证 .so/.dylib 路径的测试用。
star_runtime_shared_dep = declare_dependency(
    include_directories: starletto_inc,
    link_with: star_runtime_lib.get_shared_lib(),
    dependencies: [transport_native_dep],
)

runtime_native_dep = declare_dependency(
    include_directories: starletto_inc,
    dependencies: [runtime_api_dep, transport_native_dep, manifest_native_dep, star_runtime_dep, dl_dep],
)

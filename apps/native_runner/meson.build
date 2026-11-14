native_runner_sources = files(
    'main.cc',
    'app_run.cc',
    'app_inspect.cc',
    'app_selftest.cc',
    'app_monitor.cc',
    'app_common.cc',
)

# 静态部署的聚合 TU：模块列表来自 modules/meson.build 收集的符号。
static_declarations = ''
static_calls = ''
foreach symbol : static_component_symbols
    static_declarations = static_declarations + 'extern "C" void star_static_register_' + symbol + '(star::runtime::ComponentRegistry*);\n'
    static_calls = static_calls + '    star_static_register_' + symbol + '(registry);\n'
endforeach
static_components_config = configuration_data()
static_components_config.set('declarations', static_declarations)
static_components_config.set('calls', static_calls)
static_components_cc = configure_file(
    input: 'static_components.cc.in',
    output: 'static_components.cc',
    configuration: static_components_config,
)

executable(
    'native_runner',
    native_runner_sources,
    dependencies: [
        runtime_native_dep,
        transport_native_dep,
        manifest_native_dep,
        libraries_dep,
    ],
    install: true,
)

# 静态部署 runner：组件实现直接链进可执行文件，不经过 dlopen。
# 动态 runner 仍是默认调试路径；两者共用同一套 plan/conf。
executable(
    'native_runner_static',
    native_runner_sources + [static_components_cc],
    dependencies: [
        runtime_native_dep,
        transport_native_dep,
        manifest_native_dep,
        libraries_dep,
    ],
    link_with: static_component_static_libs,
    cpp_args: ['-DSTAR_STATIC_COMPONENTS=1'],
    install: true,
)

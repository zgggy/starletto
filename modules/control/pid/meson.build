# pid 为 header-only 组件，但按统一约定以插件形式交付：
# host 通过 component_library dlopen 加载，apps 不再静态链接组件实现
pid_dep = declare_dependency(
    include_directories: control_inc,
    dependencies: [
        star_core_dep,
        runtime_api_dep,
        transport_api_dep,
        manifest_dep,
        libraries_dep,
    ],
)

shared_library(
    'pid',
    files('register.cc'),
    dependencies: [pid_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/control/pid',
)

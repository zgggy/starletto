planning_inc = include_directories(
    '.',
    '..',
    '../..',
)

planning_dep = declare_dependency(
    include_directories: planning_inc,
    dependencies: [
        star_core_dep,
        runtime_api_dep,
        transport_api_dep,
        manifest_dep,
        libraries_dep,
    ],
)

install_data(
    'module.conf',
    install_dir: 'modules/planning',
)

subdir('motion')

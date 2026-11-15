perception_inc = include_directories(
    '.',
    '..',
    '../..',
)

perception_dep = declare_dependency(
    include_directories: perception_inc,
    dependencies: [
        star_core_dep,
        runtime_api_dep,
        transport_api_dep,
        manifest_dep,
        setting_dep,
        libraries_dep,
    ],
)

install_data(
    'module.conf',
    install_dir: 'modules/perception',
)

subdir('prediction')

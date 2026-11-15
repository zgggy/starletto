localization_inc = include_directories(
    '.',
    '..',
    '../..',
)

localization_dep = declare_dependency(
    include_directories: localization_inc,
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
    install_dir: 'modules/localization',
)

subdir('pose')

control_inc = include_directories(
    '.',
    '..',
    '../..',
)

control_dep = declare_dependency(
    include_directories: control_inc,
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
    install_dir: 'modules/control',
)

subdir('pid')

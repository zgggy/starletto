config_sources = files(
    'config_component.h',
)

config_dep = declare_dependency(
    sources: config_sources,
    dependencies: [setting_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/setting/config',
)

install_subdir(
    'files',
    install_dir: 'modules/setting/config',
)

shared_library(
    'config',
    files('register.cc'),
    dependencies: [config_dep],
    install: true,
    install_dir: 'modules/setting/config',
)

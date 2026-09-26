parameter_sources = files(
    'parameter_component.h',
    'parameter_store_service.h',
)

parameter_dep = declare_dependency(
    sources: parameter_sources,
    dependencies: [setting_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/setting/parameter',
)

install_subdir(
    'config',
    install_dir: 'modules/setting/parameter',
)

shared_library(
    'parameter',
    files('register.cc'),
    dependencies: [parameter_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/setting/parameter',
)

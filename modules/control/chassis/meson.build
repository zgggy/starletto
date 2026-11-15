chassis_sources = files('chassis_component.h')

chassis_dep = declare_dependency(
    sources: chassis_sources,
    dependencies: [control_dep, setting_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/control/chassis',
)

shared_library(
    'chassis',
    files('register.cc'),
    dependencies: [chassis_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/control/chassis',
)

chassis_static_lib = static_library(
    'chassis_static',
    files('register.cc'),
    dependencies: [chassis_dep],
    cpp_args: ['-DSTAR_STATIC_COMPONENTS_ONLY'],
)
static_component_symbols += ['control_chassis']
static_component_static_libs += [chassis_static_lib]

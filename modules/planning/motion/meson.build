motion_sources = files(
    'motion_component.h',
    'motion_processor.h',
)

motion_dep = declare_dependency(
    sources: motion_sources,
    dependencies: [planning_dep, setting_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/planning/motion',
)

shared_library(
    'motion',
    files('register.cc'),
    dependencies: [motion_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/planning/motion',
)

motion_static_lib = static_library(
    'motion_static',
    files('register.cc'),
    dependencies: [motion_dep],
    cpp_args: ['-DSTAR_STATIC_COMPONENTS_ONLY'],
)
static_component_symbols += ['planning_motion']
static_component_static_libs += [motion_static_lib]

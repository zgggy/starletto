pose_sources = files('pose_component.h')

pose_dep = declare_dependency(
    sources: pose_sources,
    dependencies: [localization_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/localization/pose',
)

shared_library(
    'pose',
    files('register.cc'),
    dependencies: [pose_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/localization/pose',
)

pose_static_lib = static_library(
    'pose_static',
    files('register.cc'),
    dependencies: [pose_dep],
    cpp_args: ['-DSTAR_STATIC_COMPONENTS_ONLY'],
)
static_component_symbols += ['localization_pose']
static_component_static_libs += [pose_static_lib]

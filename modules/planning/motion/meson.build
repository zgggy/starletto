motion_sources = files(
    'motion_component.h',
    'motion_processor.h',
)

motion_dep = declare_dependency(
    sources: motion_sources,
    dependencies: [planning_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/planning/motion',
)

shared_library(
    'motion',
    files('register.cc'),
    dependencies: [motion_dep],
    install: true,
    install_dir: 'modules/planning/motion',
)

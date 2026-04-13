pid_lib = static_library(
    'pid_controller',
    'pid_controller.h',
    'control_component.h',
    include_directories: [control_inc],
    dependencies: [
        star_core_dep,
        runtime_api_dep,
        transport_api_dep,
        manifest_dep,
        libraries_dep,
    ],
)

parameter_service_demo_sources = files(
    'main.cc',
)

executable(
    'parameter_service_demo',
    parameter_service_demo_sources,
    dependencies: [
        runtime_native_dep,
        transport_native_dep,
        manifest_native_dep,
        setting_dep,
        libraries_dep,
    ],
    install: true,
)

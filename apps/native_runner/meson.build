native_runner_sources = files(
    'main.cc',
    'app.cc',
)

executable(
    'native_runner',
    native_runner_sources,
    dependencies: [
        runtime_native_dep,
        transport_native_dep,
        manifest_native_dep,
        libraries_dep,
    ],
    install: true,
)

native_runner_sources = files(
    'main.cc',
    'app_run.cc',
    'app_inspect.cc',
    'app_selftest.cc',
    'app_monitor.cc',
    'app_common.cc',
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
    install_rpath: install_rpath,
)

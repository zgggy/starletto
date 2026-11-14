record_replay_tool_sources = files(
    'main.cc',
)

executable(
    'record_replay_tool',
    record_replay_tool_sources,
    dependencies: [
        runtime_native_dep,
        transport_native_dep,
        manifest_native_dep,
        libraries_dep,
    ],
    install: false,
)

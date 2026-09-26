# 单元测试：每个测试源文件与 support/test_main.cc 组成一个可执行目标，
# 经 `meson test` / `./star test` 运行。
#
# 测试框架见 support/star_test.h（Catch2 风格 API 子集）。

test_root_inc = include_directories('..')
test_main = files('support/test_main.cc')

test(
    'transport_percentile',
    executable(
        'transport_percentile_test',
        files('transport/percentile_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_channel_stats',
    executable(
        'transport_channel_stats_test',
        files('transport/channel_stats_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_shm_channel',
    executable(
        'transport_shm_channel_test',
        files('transport/shm_channel_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_service_concurrency',
    executable(
        'transport_service_concurrency_test',
        files('transport/service_concurrency_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'runtime_component',
    executable(
        'runtime_component_test',
        files('runtime/component_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
    ),
)

# 多 DSO 回归测试用的独立插件：只为在另一个 DSO 内生成 CorrelationId
correlation_test_plugin = shared_library(
    'correlation_test_plugin',
    files('runtime/correlation_plugin.cc'),
    include_directories: test_root_inc,
    dependencies: [transport_native_dep],
    name_suffix: 'so',
)

test(
    'runtime_correlation_id',
    executable(
        'runtime_correlation_id_test',
        files('runtime/correlation_id_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
        cpp_args: ['-DSTAR_TEST_PLUGIN_PATH="@0@"'.format(correlation_test_plugin.full_path())],
    ),
)

test(
    'runtime_health_checker',
    executable(
        'runtime_health_checker_test',
        files('runtime/health_checker_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
    ),
)

test(
    'runtime_parameter_store',
    executable(
        'runtime_parameter_store_test',
        files('runtime/parameter_store_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
    ),
)

test(
    'runtime_process_orchestrator',
    executable(
        'runtime_process_orchestrator_test',
        files('runtime/process_orchestrator_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
    ),
)

test(
    'runtime_scheduler',
    executable(
        'runtime_scheduler_test',
        files('runtime/scheduler_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
    ),
)

test(
    'runtime_trigger',
    executable(
        'runtime_trigger_test',
        files('runtime/trigger_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
    ),
)

test(
    'transport_shm_stress',
    executable(
        'transport_shm_stress_test',
        files('transport/shm_stress_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_record_replay',
    executable(
        'transport_record_replay_test',
        files('transport/record_replay_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_shm_routed',
    executable(
        'transport_shm_routed_test',
        files('transport/shm_routed_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_native_message_bus',
    executable(
        'transport_native_message_bus_test',
        files('transport/native_message_bus_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'schema_parser',
    executable(
        'schema_parser_test',
        files('schema/schema_parser_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
    ),
)

test(
    'schema_binary_codec',
    executable(
        'schema_binary_codec_test',
        files('schema/binary_codec_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
    ),
)

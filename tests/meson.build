# 单元测试：每个测试源文件与 support/test_main.cc 组成一个可执行目标，
# 经 `meson test` / `./star test` 运行。
#
# 测试框架见 support/star_test.h（Catch2 风格 API 子集）。

test_root_inc = include_directories('..')
subdir('schema/imports')
subdir('schema/wire')
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
    'transport_replay_publish',
    executable(
        'transport_replay_publish_test',
        files('transport/replay_publish_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'transport_rpc_multiprocess',
    executable(
        'transport_rpc_multiprocess_test',
        files('transport/rpc_multiprocess_test.cc') + test_main,
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
    'runtime_frame_robustness',
    executable(
        'runtime_frame_robustness_test',
        files('runtime/frame_robustness_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
    ),
    timeout: 120,
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

# 插件 ABI 契约测试用 DSO（缺指纹/指纹不一致/正常三种变体）
plugin_abi_valid = shared_library(
    'plugin_abi_valid',
    files('runtime/plugin_abi_test_plugin.cc'),
    include_directories: test_root_inc,
    dependencies: [runtime_api_dep],
    name_suffix: 'so',
)

plugin_abi_no_fingerprint = shared_library(
    'plugin_abi_no_fingerprint',
    files('runtime/plugin_abi_test_plugin.cc'),
    include_directories: test_root_inc,
    dependencies: [runtime_api_dep],
    cpp_args: ['-DSTAR_TEST_NO_FINGERPRINT'],
    name_suffix: 'so',
)

plugin_abi_wrong_fingerprint = shared_library(
    'plugin_abi_wrong_fingerprint',
    files('runtime/plugin_abi_test_plugin.cc'),
    include_directories: test_root_inc,
    dependencies: [runtime_api_dep],
    cpp_args: ['-DSTAR_TEST_WRONG_FINGERPRINT'],
    name_suffix: 'so',
)

test(
    'runtime_plugin_abi',
    executable(
        'runtime_plugin_abi_test',
        files('runtime/plugin_abi_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
        cpp_args: [
            '-DSTAR_TEST_VALID_PLUGIN_PATH="@0@"'.format(plugin_abi_valid.full_path()),
            '-DSTAR_TEST_NO_FINGERPRINT_PLUGIN_PATH="@0@"'.format(plugin_abi_no_fingerprint.full_path()),
            '-DSTAR_TEST_WRONG_FINGERPRINT_PLUGIN_PATH="@0@"'.format(plugin_abi_wrong_fingerprint.full_path()),
        ],
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
    'runtime_health_monitor',
    executable(
        'runtime_health_monitor_test',
        files('runtime/health_monitor_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
    ),
)

test(
    'runtime_resource_monitor',
    executable(
        'runtime_resource_monitor_test',
        files('runtime/resource_monitor_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
    ),
)

test(
    'runtime_stats_publisher',
    executable(
        'runtime_stats_publisher_test',
        files('runtime/stats_publisher_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
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
    'runtime_typed_component',
    executable(
        'runtime_typed_component_test',
        files('runtime/typed_component_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep],
    ),
)

test(
    'runtime_plan',
    executable(
        'runtime_plan_test',
        files('runtime/plan_test.cc') + test_main,
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
    'core_logger',
    executable(
        'core_logger_test',
        files('core/logger_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [star_core_dep, libraries_dep],
    ),
)

# P2-4：多 root schema 的生成与身份测试（.schema 只用于测试，不参与业务）
multi_root_schema = custom_target(
    'gen_test_multi_root_schema',
    input: files('schema/multi_root.schema'),
    output: ['multi_root.s.h'],
    command: [
        codegen_py,
        codegen_script,
        'gen',
        '--schema', meson.current_source_dir() / 'schema' / 'multi_root.schema',
        '--output', meson.current_build_dir() / 'multi_root.s.h',
    ],
    depend_files: codegen_sources,
    build_by_default: true,
)

test(
    'runtime_plugin_schema',
    executable(
        'runtime_plugin_schema_test',
        files('runtime/plugin_schema_test.cc') + test_main + [parameter_plugin],
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
        cpp_args: ['-DSTAR_TEST_PARAMETER_PLUGIN_PATH="@0@"'.format(parameter_plugin.full_path())],
    ),
)

test(
    'runtime_wire_identity',
    executable(
        'runtime_wire_identity_test',
        files('runtime/wire_identity_test.cc') + test_main + [wire_schemas_gen],
        include_directories: test_root_inc,
        dependencies: [runtime_native_dep, libraries_dep],
    ),
)

test(
    'schema_import',
    executable(
        'schema_import_test',
        files('schema/import_test.cc') + test_main + [import_schemas_gen],
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
    ),
)

test(
    'schema_multi_root',
    executable(
        'schema_multi_root_test',
        files('schema/multi_root_test.cc') + test_main + [multi_root_schema],
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
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

test(
    'transport_shm_payload_encoding',
    executable(
        'transport_shm_payload_encoding_test',
        files('transport/shm_payload_encoding_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [transport_native_dep],
    ),
)

test(
    'schema_fixed_layout',
    executable(
        'schema_fixed_layout_test',
        files('schema/fixed_layout_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
    ),
)

test(
    'schema_robustness',
    executable(
        'schema_robustness_test',
        files('schema/robustness_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
    ),
    timeout: 120,
)

test(
    'schema_registry',
    executable(
        'schema_registry_test',
        files('schema/schema_registry_test.cc') + test_main,
        include_directories: test_root_inc,
        dependencies: [schema_dep, libraries_dep],
    ),
)

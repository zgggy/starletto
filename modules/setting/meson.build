setting_inc = include_directories(
    '.',
    '..',
    '../..',
)

# codegen：.schema -> .s.h 由 custom_target 处理
subdir('schema')

setting_dep = declare_dependency(
    include_directories: setting_inc,
    sources: [
        schema_gen_modules_setting_schema,
        schema_gen_modules_setting_schema_config,
        schema_gen_modules_setting_schema_control,
        schema_gen_modules_setting_schema_planning,
        schema_gen_modules_setting_schema_service,
    ],
    dependencies: [
        star_core_dep,
        runtime_api_dep,
        transport_api_dep,
        manifest_dep,
        libraries_dep,
    ],
)

install_data(
    'module.conf',
    install_dir: 'modules/setting',
)

subdir('config')
subdir('parameter')

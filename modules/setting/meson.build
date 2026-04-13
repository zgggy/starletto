setting_inc = include_directories(
    '.',
    '..',
    '../..',
)

codegen_py = find_program('python3', required: true)
codegen_script = files('../../basement/schema/schema_codegen.py')

base_dir = meson.current_source_dir()

run_command(
    codegen_py,
    [codegen_script, 'gen', '--schema-dir', base_dir / 'schema', '--output-dir', base_dir / 'schema'],
    check: true,
)

setting_dep = declare_dependency(
    include_directories: setting_inc,
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

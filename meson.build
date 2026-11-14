project(
    'starletto',
    'cpp',
    version: '0.1.0',
    default_options: [
        'cpp_std=c++20',
        'warning_level=3',
        'buildtype=debugoptimized',
    ],
)

# 工程级告警门禁：下列告警各自都曾造成真实缺陷（失效的 include guard、
# 空体非 void 函数、前向声明与定义不符、漏写 override），提为错误以防回归。
# 经 get_supported_arguments 过滤，GCC/Clang 差异（suggest-override 等）不影响构建。
cpp = meson.get_compiler('cpp')
add_project_arguments(
    cpp.get_supported_arguments([
        '-Werror=return-type',
        '-Werror=header-guard',
        '-Werror=mismatched-tags',
        '-Werror=inconsistent-missing-override',
        '-Werror=suggest-override',
    ]),
    language: 'cpp',
)

subdir('basement')
subdir('libraries')
subdir('modules')
subdir('apps')
subdir('tests')

# ── 安装面：公共头 + 生成的 builtin 头 + schema 工具 + pkg-config ──────────
# install_subdir 保留 basement/... 的目录结构；安装脚本随后删除 .h/.hpp 之外的
# 文件（meson.build/.schema/Python），避免 include 树变成第二份源码树。
install_subdir('basement', install_dir: get_option('includedir') / 'starletto')
install_subdir('libraries', install_dir: get_option('includedir') / 'starletto')
meson.add_install_script('tools/install_surface/prune_include_tree.py', get_option('includedir') / 'starletto')

# schema 工具：源码树外的模块用它生成自己的 .s.h
install_data(
    'basement/schema/schema_codegen.py',
    'basement/schema/schema_compat.py',
    install_dir: get_option('datadir') / 'starletto' / 'schema',
)
install_subdir(
    'basement/schema/codegen',
    install_dir: get_option('datadir') / 'starletto' / 'schema' / 'codegen',
    strip_directory: true,
    exclude_directories: ['__pycache__'],
)

pkgconfig = import('pkgconfig')
pkgconfig.generate(
    name: 'starletto',
    description: 'Starletto native runtime (header-only)',
    version: meson.project_version(),
    subdirs: 'starletto',
    extra_cflags: ['-std=c++20'],
    variables: [
        'schemadir=${prefix}/share/starletto/schema',
    ],
)

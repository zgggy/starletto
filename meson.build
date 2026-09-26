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

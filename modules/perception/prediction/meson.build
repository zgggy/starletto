prediction_sources = files('prediction_component.h')

prediction_dep = declare_dependency(
    sources: prediction_sources,
    dependencies: [perception_dep],
)

install_data(
    'component.conf',
    install_dir: 'modules/perception/prediction',
)

shared_library(
    'prediction',
    files('register.cc'),
    dependencies: [prediction_dep],
    name_suffix: 'so',
    install: true,
    install_dir: 'modules/perception/prediction',
)

prediction_static_lib = static_library(
    'prediction_static',
    files('register.cc'),
    dependencies: [prediction_dep],
    cpp_args: ['-DSTAR_STATIC_COMPONENTS_ONLY'],
)
static_component_symbols += ['perception_prediction']
static_component_static_libs += [prediction_static_lib]

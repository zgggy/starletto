#include "modules/setting/parameter/parameter_component.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    star::setting::register_native_parameter_components(*registry);
}


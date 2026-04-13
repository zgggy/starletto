#include "modules/setting/config/config_component.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry& registry) {
    star::setting::register_native_config_components(registry);
}


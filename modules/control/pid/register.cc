#include "basement/core/build_info.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/control/pid/control_component.h"

extern "C" void star_static_register_control_pid(star::runtime::ComponentRegistry* registry) {
    if (registry != nullptr) {
        star::control::register_control_components(*registry);
    }
}

#ifndef STAR_STATIC_COMPONENTS_ONLY
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star_static_register_control_pid(registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
#endif

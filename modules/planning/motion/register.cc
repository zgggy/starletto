#include "basement/core/build_info.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/planning/motion/motion_component.h"

extern "C" void star_static_register_planning_motion(star::runtime::ComponentRegistry* registry) {
    if (registry != nullptr) {
        star::planning::register_native_components(*registry);
    }
}

#ifndef STAR_STATIC_COMPONENTS_ONLY
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star_static_register_planning_motion(registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
#endif

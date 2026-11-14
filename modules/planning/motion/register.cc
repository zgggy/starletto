#include "basement/core/build_info.h"
#include "basement/runtime/component/component_registry.h"
#include "modules/planning/motion/motion_component.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star::planning::register_native_components(*registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()

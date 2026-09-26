#include "basement/runtime/component/component_registry.h"
#include "modules/control/pid/control_component.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star::control::register_control_components(*registry); }

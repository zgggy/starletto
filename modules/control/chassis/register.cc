#include "basement/core/build_info.h"
#include "basement/core/exception.h"
#include "modules/control/chassis/chassis_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

extern "C" void star_static_register_control_chassis(star::runtime::ComponentRegistry* registry) {
    if (registry == nullptr || registry->schema_registry() == nullptr) {
        throw star::excep::RuntimeError("control/chassis loaded without a SchemaRegistry");
    }
    star::setting::schema::register_setting_schemas(*registry->schema_registry());
    star::control::register_chassis_components(*registry);
}

#ifndef STAR_STATIC_COMPONENTS_ONLY
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star_static_register_control_chassis(registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
#endif

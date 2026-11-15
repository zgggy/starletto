#include "basement/core/build_info.h"
#include "basement/core/exception.h"
#include "modules/localization/pose/pose_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

extern "C" void star_static_register_localization_pose(star::runtime::ComponentRegistry* registry) {
    if (registry == nullptr || registry->schema_registry() == nullptr) {
        throw star::excep::RuntimeError("localization/pose loaded without a SchemaRegistry");
    }
    star::setting::schema::register_setting_schemas(*registry->schema_registry());
    star::localization::register_pose_components(*registry);
}

#ifndef STAR_STATIC_COMPONENTS_ONLY
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star_static_register_localization_pose(registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
#endif

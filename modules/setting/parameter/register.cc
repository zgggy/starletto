#include "basement/core/build_info.h"
#include "basement/core/exception.h"
#include "modules/setting/parameter/parameter_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

extern "C" void star_static_register_setting_parameter(star::runtime::ComponentRegistry* registry) {
    if (registry == nullptr || registry->schema_registry() == nullptr) {
        throw star::excep::RuntimeError("setting/parameter loaded without a SchemaRegistry");
    }
    // config/parameter 任一被装配都保证 setting schema 可用（注册幂等）
    star::setting::schema::register_setting_schemas(*registry->schema_registry());
    star::setting::register_native_parameter_components(*registry);
}

#ifndef STAR_STATIC_COMPONENTS_ONLY
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star_static_register_setting_parameter(registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
#endif

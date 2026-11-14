#include "basement/core/build_info.h"
#include "basement/core/exception.h"
#include "modules/setting/config/config_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

extern "C" void star_static_register_setting_config(star::runtime::ComponentRegistry* registry) {
    if (registry == nullptr || registry->schema_registry() == nullptr) {
        throw star::excep::RuntimeError("setting/config loaded without a SchemaRegistry");
    }
    // 静态/插件两种装配共用同一入口：schema 由模块自己注册
    star::setting::schema::register_setting_schemas(*registry->schema_registry());
    star::setting::register_native_config_components(*registry);
}

#ifndef STAR_STATIC_COMPONENTS_ONLY
extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) { star_static_register_setting_config(registry); }
STAR_EXPORT_TOOLCHAIN_FINGERPRINT()
#endif

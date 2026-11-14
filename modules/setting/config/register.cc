#include "basement/core/exception.h"
#include "modules/setting/config/config_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    if (registry == nullptr || registry->schema_registry() == nullptr) {
        throw star::excep::RuntimeError("setting/config plugin loaded without a SchemaRegistry");
    }
    // 插件自注册：模块的 schema 由插件入口负责，app 不再知道 setting 的存在
    star::setting::schema::register_setting_schemas(*registry->schema_registry());
    star::setting::register_native_config_components(*registry);
}

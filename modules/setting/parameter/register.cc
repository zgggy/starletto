#include "basement/core/exception.h"
#include "modules/setting/parameter/parameter_component.h"
#include "modules/setting/schema/schema_bootstrap.h"

extern "C" void star_register_components(star::runtime::ComponentRegistry* registry) {
    if (registry == nullptr || registry->schema_registry() == nullptr) {
        throw star::excep::RuntimeError("setting/parameter plugin loaded without a SchemaRegistry");
    }
    // 插件自注册：config/parameter 任一被加载都保证 setting schema 可用（注册幂等）
    star::setting::schema::register_setting_schemas(*registry->schema_registry());
    star::setting::register_native_parameter_components(*registry);
}

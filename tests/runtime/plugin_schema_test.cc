#include <dlfcn.h>

#include <string>

#include "basement/core/build_info.h"
#include "basement/core/exception.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/schema/schema_registry.h"

#include "tests/support/star_test.h"

// P1.2：插件入口负责注册自己的 schema；app 不再知道业务模块的存在
TEST_CASE("plugin entry registers its module schemas") {
    const auto path = std::string(STAR_TEST_PARAMETER_PLUGIN_PATH);
    void* handle = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    REQUIRE(handle != nullptr);
    ::dlerror();
    const auto symbol = ::dlsym(handle, "star_register_components");
    REQUIRE(symbol != nullptr);
    REQUIRE(::dlerror() == nullptr);

    auto registry = star::runtime::ComponentRegistry{};
    auto schemas = star::schema::SchemaRegistry{};
    registry.set_schema_registry(&schemas);
    reinterpret_cast<void (*)(star::runtime::ComponentRegistry*)>(symbol)(&registry);

    const auto fingerprint = ::dlsym(handle, "star_toolchain_fingerprint");
    REQUIRE(fingerprint != nullptr);
    REQUIRE_EQ(std::string(reinterpret_cast<const char* (*)()>(fingerprint)()), std::string(star::base::toolchain_fingerprint()));

    REQUIRE(registry.contains("star::parameter::Parameter"));
    REQUIRE(schemas.find_latest("planning.motion").has_value());
    REQUIRE(schemas.find_latest("setting.parameter.service.get_request").has_value());

    ::dlclose(handle);
}

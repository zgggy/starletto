#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

#include "basement/runtime/component/component_dl_helper.h"
#include "basement/runtime/component/component_registry.h"

#include "tests/support/star_test.h"

namespace {

using star::generated::runtime::component_load_plan::ComponentLoadPlan;
using star::runtime::ComponentDlHelper;
using star::runtime::ComponentRegistry;
using star::runtime::DlHandle;

#ifndef STAR_TEST_VALID_PLUGIN_PATH
#error "STAR_TEST_VALID_PLUGIN_PATH must be defined by the build"
#endif
#ifndef STAR_TEST_NO_FINGERPRINT_PLUGIN_PATH
#error "STAR_TEST_NO_FINGERPRINT_PLUGIN_PATH must be defined by the build"
#endif
#ifndef STAR_TEST_WRONG_FINGERPRINT_PLUGIN_PATH
#error "STAR_TEST_WRONG_FINGERPRINT_PLUGIN_PATH must be defined by the build"
#endif

auto make_plan(const std::string& plugin_path) -> ComponentLoadPlan {
    auto plan = ComponentLoadPlan{};
    plan.module_name = "abi";
    plan.component_name = "probe";
    plan.component_class = "star::test::AbiProbe";
    const auto path = std::filesystem::path(plugin_path);
    plan.module_dir = path.parent_path().string();
    plan.component_library = path.filename().string();
    return plan;
}

auto load(const std::string& plugin_path, ComponentRegistry& registry, std::unordered_map<std::string, DlHandle>& handles) -> void {
    auto loader = ComponentDlHelper{};
    loader.load_dynamic_library(make_plan(plugin_path), handles, &registry);
}

}  // namespace

TEST_CASE("plugin with matching toolchain fingerprint loads") {
    // handles 必须先于 registry 声明：registry 的工厂 lambda 代码在插件 DSO 内，
    // dlclose 之后才能析构 registry（NativeComponentHost 的成员顺序同理）。
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto registry = std::make_shared<ComponentRegistry>();
    load(STAR_TEST_VALID_PLUGIN_PATH, *registry, handles);

    REQUIRE(registry->contains("star::test::AbiProbe"));
    auto component = registry->create("star::test::AbiProbe");
    REQUIRE(component != nullptr);
    REQUIRE(component->process_once());
}

TEST_CASE("plugin without toolchain fingerprint is rejected") {
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto registry = std::make_shared<ComponentRegistry>();
    auto threw = false;
    try {
        load(STAR_TEST_NO_FINGERPRINT_PLUGIN_PATH, *registry, handles);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
    REQUIRE(not registry->contains("star::test::AbiProbe"));
}

TEST_CASE("plugin with mismatched toolchain fingerprint is rejected") {
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto registry = std::make_shared<ComponentRegistry>();
    auto threw = false;
    try {
        load(STAR_TEST_WRONG_FINGERPRINT_PLUGIN_PATH, *registry, handles);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
    REQUIRE(not registry->contains("star::test::AbiProbe"));
}

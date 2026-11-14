#include <memory>
#include <string>
#include <vector>

#include "basement/runtime/parameter/native_parameter_store.h"
#include "basement/schema/schema_value.h"

#include "tests/support/star_test.h"

namespace {

using star::runtime::NativeParameterStore;
using star::schema::SchemaValue;

TEST_CASE("NativeParameterStore roundtrips typed values") {
    auto store = NativeParameterStore{};

    store.set("planning.cycle_ms", SchemaValue{int64_t{100}});
    store.set("planning.name", SchemaValue{std::string{"lattice"}});
    store.set("control.enable", SchemaValue{true});

    // 必须先把 SchemaValue 存下来：as_scalar() 返回内部引用，直接对临时对象取值
    // 会在 REQUIRE_EQ 比较前销毁临时对象（ASan: stack-use-after-scope）
    const auto cycle_ms = store.get("planning.cycle_ms");
    const auto name = store.get("planning.name");
    const auto enable = store.get("control.enable");
    REQUIRE_EQ(std::get<int64_t>(cycle_ms.as_scalar()), int64_t{100});
    REQUIRE_EQ(std::get<std::string>(name.as_scalar()), std::string{"lattice"});
    REQUIRE_EQ(std::get<bool>(enable.as_scalar()), true);
}

TEST_CASE("NativeParameterStore get of a missing key returns null") {
    auto store = NativeParameterStore{};
    const auto value = store.get("missing.key");

    REQUIRE(value.is_scalar());
    REQUIRE(std::holds_alternative<std::nullptr_t>(value.as_scalar()));
}

TEST_CASE("NativeParameterStore list filters by prefix") {
    auto store = NativeParameterStore{};
    store.set("planning.motion.cycle_ms", SchemaValue{int64_t{100}});
    store.set("planning.motion.method", SchemaValue{std::string{"lattice"}});
    store.set("planning.refline.step", SchemaValue{int64_t{5}});
    store.set("control.pid.kp", SchemaValue{double{1.5}});

    const auto planning = store.list("planning.motion");
    REQUIRE_EQ(planning.size(), 2U);
    for (const auto& key : planning) {
        CHECK(key.find("planning.motion") == 0);
    }

    REQUIRE_EQ(store.list("control").size(), 1U);
    REQUIRE_EQ(store.list("nothing").size(), 0U);
}

TEST_CASE("NativeParameterStore set overwrites the previous value") {
    auto store = NativeParameterStore{};
    store.set("k", SchemaValue{int64_t{1}});
    store.set("k", SchemaValue{int64_t{42}});

    const auto value = store.get("k");
    REQUIRE_EQ(std::get<int64_t>(value.as_scalar()), int64_t{42});
    REQUIRE_EQ(store.list("").size(), 1U);
}

}  // namespace

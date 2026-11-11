#include <cstdint>
#include <string>
#include <vector>

#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/builtin/runtime/process_plan.s.h"
#include "basement/schema/schema_registry.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::FieldDescriptor;
using star::schema::FieldKind;
using star::schema::SchemaDescriptor;
using star::schema::SchemaMatchStatus;
using star::schema::SchemaRegistry;
using star::schema::SchemaTypeId;

auto make_schema(const std::string& name, int64_t version, uint64_t hash, int64_t field_id) -> SchemaDescriptor {
    auto out = SchemaDescriptor{};
    out.type_id = SchemaTypeId{name, version, hash};
    out.namespace_name = "test";
    out.root_name = "Root";

    auto field = FieldDescriptor{};
    field.id = static_cast<uint32_t>(field_id);
    field.name = "value";
    field.kind = FieldKind::INT64;
    field.type_name = "int";
    out.fields.push_back(field);
    return out;
}

template <typename _Fn>
auto throws_runtime_error(_Fn&& fn) -> bool {
    try {
        fn();
    } catch (const star::excep::RuntimeError&) {
        return true;
    }
    return false;
}

}  // namespace

// 决策二的存储基座：同名不同 version 必须共存，否则滚动升级时新旧进程注册同一类型会互相踢掉
TEST_CASE("SchemaRegistry stores multiple versions of the same schema name") {
    auto registry = SchemaRegistry{};
    registry.register_schema(make_schema("test.msg", 1, 0x1111, 1));
    registry.register_schema(make_schema("test.msg", 2, 0x2222, 2));

    REQUIRE_EQ(registry.size(), 2U);

    const auto versions = registry.versions("test.msg");
    REQUIRE_EQ(versions.size(), 2U);
    REQUIRE_EQ(versions[0], int64_t{1});
    REQUIRE_EQ(versions[1], int64_t{2});

    const auto v1 = registry.find_by_name_and_version("test.msg", 1);
    REQUIRE(v1.has_value());
    REQUIRE_EQ(v1->type_id.hash, uint64_t{0x1111});

    // find_latest 是「未做多版本决策时」的默认语义：取最高版本
    const auto latest = registry.find_latest("test.msg");
    REQUIRE(latest.has_value());
    REQUIRE_EQ(latest->type_id.version, int64_t{2});
}

// 重复注册同一份 schema 是正常路径（每个插件 DSO 都会 register 一次），必须幂等
TEST_CASE("SchemaRegistry treats duplicate identical registration as idempotent") {
    auto registry = SchemaRegistry{};
    registry.register_schema(make_schema("test.msg", 1, 0x1111, 1));
    registry.register_schema(make_schema("test.msg", 1, 0x1111, 1));

    REQUIRE_EQ(registry.size(), 1U);
}

// 同 name+version 不同内容 = 契约冲突，必须报错而不是各存一份
TEST_CASE("SchemaRegistry rejects same name+version with different content hash") {
    auto registry = SchemaRegistry{};
    registry.register_schema(make_schema("test.msg", 1, 0x1111, 1));

    const auto rejected = throws_runtime_error([&registry] {
        registry.register_schema(make_schema("test.msg", 1, 0x9999, 1));
    });
    REQUIRE(rejected);
    REQUIRE_EQ(registry.size(), 1U);
}

// 手工构造的 descriptor（hash=0）没有指纹可用，退回结构比较——不同字段仍然算冲突
TEST_CASE("SchemaRegistry falls back to structural comparison when hash is unknown") {
    auto registry = SchemaRegistry{};
    registry.register_schema(make_schema("test.msg", 1, 0, 1));

    const auto rejected = throws_runtime_error([&registry] {
        registry.register_schema(make_schema("test.msg", 1, 0, 2));
    });
    REQUIRE(rejected);

    // 结构相同则幂等
    registry.register_schema(make_schema("test.msg", 1, 0, 1));
    REQUIRE_EQ(registry.size(), 1U);
}

// lookup 必须把「没找到」「hash 未知」「内容不一致」分开——阶段三的解码策略按这个状态分支
TEST_CASE("SchemaRegistry lookup distinguishes mismatch from missing schema") {
    auto registry = SchemaRegistry{};
    registry.register_schema(make_schema("test.msg", 1, 0x1111, 1));

    const auto exact = registry.lookup(SchemaTypeId{"test.msg", 1, 0x1111});
    REQUIRE(exact.status == SchemaMatchStatus::kExact);
    REQUIRE(exact.schema.has_value());

    const auto unknown = registry.lookup(SchemaTypeId{"test.msg", 1, 0});
    REQUIRE(unknown.status == SchemaMatchStatus::kHashUnknown);
    REQUIRE(unknown.schema.has_value());

    const auto mismatch = registry.lookup(SchemaTypeId{"test.msg", 1, 0x2222});
    REQUIRE(mismatch.status == SchemaMatchStatus::kHashMismatch);
    // 冲突时仍然返回已知的 schema，供上层打印「他方 hash / 我方 hash」做诊断
    REQUIRE(mismatch.schema.has_value());

    const auto missing = registry.lookup(SchemaTypeId{"test.msg", 9, 0x1111});
    REQUIRE(missing.status == SchemaMatchStatus::kNotFound);
    REQUIRE(!missing.schema.has_value());
}

TEST_CASE("SchemaRegistry find_by_type_id rejects hash mismatch") {
    auto registry = SchemaRegistry{};
    registry.register_schema(make_schema("test.msg", 1, 0x1111, 1));

    REQUIRE(registry.find_by_type_id(SchemaTypeId{"test.msg", 1, 0x1111}).has_value());
    REQUIRE(registry.find_by_type_id(SchemaTypeId{"test.msg", 1, 0}).has_value());
    REQUIRE(!registry.find_by_type_id(SchemaTypeId{"test.msg", 1, 0x2222}).has_value());
    REQUIRE(!registry.find_by_type_id(SchemaTypeId{"test.msg", 2, 0x1111}).has_value());
}

// codegen 的 hash 必须真实：若仍是手写 0，跨 DSO 的同名同版本不同内容将无法被发现
TEST_CASE("generated schema type ids carry a real content hash") {
    using LoadPlan = star::generated::runtime::component_load_plan::ComponentLoadPlan;
    using ProcessPlan = star::generated::runtime::process_plan::ProcessPlan;

    const auto load_plan_id = LoadPlan::schema_type_id();
    REQUIRE_EQ(load_plan_id.name, std::string("runtime.component_load_plan"));
    REQUIRE_EQ(load_plan_id.version, int64_t{1});
    REQUIRE(load_plan_id.hash != 0);

    const auto process_plan_id = ProcessPlan::schema_type_id();
    REQUIRE(process_plan_id.hash != 0);
    REQUIRE(process_plan_id.hash != load_plan_id.hash);

    auto registry = SchemaRegistry{};
    LoadPlan::register_schema(registry);
    ProcessPlan::register_schema(registry);
    REQUIRE_EQ(registry.size(), 2U);
    REQUIRE(registry.lookup(load_plan_id).status == SchemaMatchStatus::kExact);
}

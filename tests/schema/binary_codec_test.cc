#include <cstdint>
#include <string>
#include <vector>

#include "basement/schema/binary_codec.h"
#include "basement/schema/native/native_schema_loader.h"
#include "basement/schema/schema_validator.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::BinaryCodec;
using star::schema::DecodedMessage;
using star::schema::FieldKind;
using star::schema::NativeSchemaLoader;
using star::schema::SchemaArray;
using star::schema::SchemaObject;
using star::schema::SchemaValue;
using star::schema::SchemaValidator;

constexpr const char* kSchemaText = R"SCHEMA(
schema test.sample

root Sample {
    1 id : int
    2 name : string
    3 ratio : float ? = 0.5
    4 enabled : bool ? = true
    5 tags : string[] ?
}
)SCHEMA";

// 在 Sample 之上多一个字段，用于验证 unknown field 兼容保留
constexpr const char* kSchemaTextV2 = R"SCHEMA(
schema test.sample

root Sample {
    1 id : int
    2 name : string
    3 ratio : float ? = 0.5
    4 enabled : bool ? = true
    5 tags : string[] ?
    6 note : string ?
}
)SCHEMA";

auto load(const char* text) -> star::schema::SchemaDescriptor { return NativeSchemaLoader{}.load_schema_text(text); }

auto make_sample() -> SchemaValue {
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{42}};
    obj["name"] = SchemaValue{std::string{"star"}};
    obj["ratio"] = SchemaValue{1.5};
    obj["enabled"] = SchemaValue{true};
    auto tags = SchemaArray{};
    tags.push_back(SchemaValue{std::string{"alpha"}});
    tags.push_back(SchemaValue{std::string{"beta"}});
    obj["tags"] = SchemaValue{tags};
    return SchemaValue{obj};
}

TEST_CASE("schema parser describes the root and its fields") {
    const auto schema = load(kSchemaText);
    REQUIRE_EQ(schema.root_name, std::string("Sample"));
    REQUIRE_EQ(schema.fields.size(), 5U);

    REQUIRE_EQ(schema.fields[0].id, 1U);
    REQUIRE_EQ(schema.fields[0].name, std::string("id"));
    REQUIRE(schema.fields[0].kind == FieldKind::INT64);
    REQUIRE(not schema.fields[0].optional);

    REQUIRE(schema.fields[1].kind == FieldKind::STRING);
    REQUIRE(schema.fields[2].kind == FieldKind::FLOAT64);
    REQUIRE(schema.fields[3].kind == FieldKind::BOOL);
    REQUIRE(schema.fields[2].optional);
    REQUIRE(schema.fields[3].optional);

    REQUIRE(schema.fields[4].repeated);
}

TEST_CASE("binary codec roundtrips every scalar kind plus a string array") {
    const auto schema = load(kSchemaText);
    const auto bytes = BinaryCodec::encode(schema, make_sample());
    REQUIRE(not bytes.empty());

    const auto decoded = BinaryCodec::decode(schema, bytes);
    REQUIRE(decoded.is_object());
    const auto& obj = decoded.as_object();

    REQUIRE_EQ(std::get<int64_t>(obj.at("id").as_scalar()), int64_t{42});
    REQUIRE_EQ(std::get<std::string>(obj.at("name").as_scalar()), std::string{"star"});
    REQUIRE_EQ(std::get<double>(obj.at("ratio").as_scalar()), 1.5);
    REQUIRE_EQ(std::get<bool>(obj.at("enabled").as_scalar()), true);

    const auto& tags = obj.at("tags").as_array();
    REQUIRE_EQ(tags.size(), 2U);
    REQUIRE_EQ(std::get<std::string>(tags[0].as_scalar()), std::string{"alpha"});
    REQUIRE_EQ(std::get<std::string>(tags[1].as_scalar()), std::string{"beta"});
}

TEST_CASE("binary codec roundtrips an empty object") {
    const auto schema = load(kSchemaText);
    const auto bytes = BinaryCodec::encode(schema, SchemaValue{SchemaObject{}});
    const auto decoded = BinaryCodec::decode(schema, bytes);
    REQUIRE(decoded.is_object());
}

TEST_CASE("apply_defaults fills optional fields that were omitted") {
    const auto schema = load(kSchemaText);
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{7}};
    obj["name"] = SchemaValue{std::string{"bare"}};
    auto value = SchemaValue{obj};

    SchemaValidator::apply_defaults(schema, &value);
    SchemaValidator::validate(schema, value);

    const auto& out = value.as_object();
    REQUIRE_EQ(std::get<double>(out.at("ratio").as_scalar()), 0.5);
    REQUIRE_EQ(std::get<bool>(out.at("enabled").as_scalar()), true);
}

TEST_CASE("validate rejects a missing required field") {
    const auto schema = load(kSchemaText);
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{7}};
    const auto value = SchemaValue{obj};

    auto threw = false;
    try {
        SchemaValidator::validate(schema, value);
    } catch (const std::exception&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("decode keeps unknown fields from a newer schema") {
    const auto schema_v2 = load(kSchemaTextV2);
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    obj["name"] = SchemaValue{std::string{"legacy"}};
    obj["note"] = SchemaValue{std::string{"from the future"}};
    const auto bytes = BinaryCodec::encode(schema_v2, SchemaValue{obj});

    // 用旧 schema 解码：新增的 note 应被保留为 unknown field
    const auto schema_v1 = load(kSchemaText);
    const auto decoded = BinaryCodec::decode_with_unknown(schema_v1, bytes);

    REQUIRE_EQ(decoded.unknown_fields.size(), 1U);
    REQUIRE_EQ(decoded.unknown_fields.front().field_id, 6U);
    REQUIRE(not decoded.unknown_fields.front().tlv_bytes.empty());

    const auto& out = decoded.value.as_object();
    REQUIRE_EQ(std::get<int64_t>(out.at("id").as_scalar()), int64_t{1});
    REQUIRE_EQ(std::get<std::string>(out.at("name").as_scalar()), std::string{"legacy"});
    REQUIRE(out.count("note") == 0);
}

TEST_CASE("unknown fields survive a decode and re-encode cycle") {
    const auto schema_v2 = load(kSchemaTextV2);
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    obj["name"] = SchemaValue{std::string{"legacy"}};
    obj["note"] = SchemaValue{std::string{"from the future"}};
    const auto original = BinaryCodec::encode(schema_v2, SchemaValue{obj});

    const auto schema_v1 = load(kSchemaText);
    const auto first = BinaryCodec::decode_with_unknown(schema_v1, original);
    const auto reencoded = BinaryCodec::encode(schema_v1, first.value, first.unknown_fields);

    const auto second = BinaryCodec::decode_with_unknown(schema_v2, reencoded);
    REQUIRE_EQ(second.unknown_fields.size(), 0U);
    REQUIRE_EQ(std::get<std::string>(second.value.as_object().at("note").as_scalar()), std::string{"from the future"});
}

}  // namespace

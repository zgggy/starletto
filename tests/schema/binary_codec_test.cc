#include <cstdint>
#include <string>
#include <vector>

#include "basement/schema/binary_codec.h"
#include "basement/schema/schema_validator.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::BinaryCodec;
using star::schema::DecodedMessage;
using star::schema::FieldKind;
using star::schema::SchemaArray;
using star::schema::SchemaObject;
using star::schema::SchemaValue;
using star::schema::SchemaValidator;

auto make_field(uint32_t id, std::string name, star::schema::FieldKind kind) -> star::schema::FieldDescriptor {
    auto field = star::schema::FieldDescriptor{};
    field.id = id;
    field.name = std::move(name);
    field.kind = kind;
    return field;
}

auto make_sample_v1() -> star::schema::SchemaDescriptor {
    auto schema = star::schema::SchemaDescriptor{};
    schema.type_id = star::schema::SchemaTypeId{"test.sample", 1, 0};
    schema.root_name = "Sample";

    schema.fields.push_back(make_field(1, "id", star::schema::FieldKind::INT64));
    schema.fields.push_back(make_field(2, "name", star::schema::FieldKind::STRING));

    auto ratio = make_field(3, "ratio", star::schema::FieldKind::FLOAT64);
    ratio.optional = true;
    ratio.has_default = true;
    ratio.default_value = SchemaValue{0.5};
    schema.fields.push_back(ratio);

    auto enabled = make_field(4, "enabled", star::schema::FieldKind::BOOL);
    enabled.optional = true;
    enabled.has_default = true;
    enabled.default_value = SchemaValue{true};
    schema.fields.push_back(enabled);

    auto tags = make_field(5, "tags", star::schema::FieldKind::STRING);
    tags.optional = true;
    tags.repeated = true;
    schema.fields.push_back(tags);
    return schema;
}

auto make_sample_v2() -> star::schema::SchemaDescriptor {
    auto schema = make_sample_v1();
    schema.type_id.version = 2;
    auto note = make_field(6, "note", star::schema::FieldKind::STRING);
    note.optional = true;
    schema.fields.push_back(note);
    return schema;
}

auto make_typed(int64_t version, star::schema::FieldKind kind) -> star::schema::SchemaDescriptor {
    auto schema = star::schema::SchemaDescriptor{};
    schema.type_id = star::schema::SchemaTypeId{"test.typed", version, 0};
    schema.root_name = "Typed";
    schema.fields.push_back(make_field(1, "value", kind));
    return schema;
}

auto make_missing_v1() -> star::schema::SchemaDescriptor {
    auto schema = star::schema::SchemaDescriptor{};
    schema.type_id = star::schema::SchemaTypeId{"test.missing", 1, 0};
    schema.root_name = "Missing";
    schema.fields.push_back(make_field(1, "id", star::schema::FieldKind::INT64));
    return schema;
}

auto make_missing_v2() -> star::schema::SchemaDescriptor {
    auto schema = star::schema::SchemaDescriptor{};
    schema.type_id = star::schema::SchemaTypeId{"test.missing", 2, 0};
    schema.root_name = "Missing";
    schema.fields.push_back(make_field(1, "id", star::schema::FieldKind::INT64));
    auto note = make_field(2, "note", star::schema::FieldKind::STRING);
    note.has_default = true;
    note.default_value = SchemaValue{std::string{"fallback"}};
    schema.fields.push_back(note);
    return schema;
}

auto make_required_repeated() -> star::schema::SchemaDescriptor {
    auto schema = star::schema::SchemaDescriptor{};
    schema.type_id = star::schema::SchemaTypeId{"test.repeat", 1, 0};
    schema.root_name = "Repeat";
    schema.fields.push_back(make_field(1, "id", star::schema::FieldKind::INT64));
    auto tags = make_field(2, "tags", star::schema::FieldKind::STRING);
    tags.repeated = true;
    schema.fields.push_back(tags);
    return schema;
}

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

TEST_CASE("schema descriptor describes the root and its fields") {
    const auto schema = make_sample_v1();
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
    const auto schema = make_sample_v1();
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

TEST_CASE("binary codec roundtrips a complete object") {
    const auto schema = make_sample_v1();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{7}};
    obj["name"] = SchemaValue{std::string{"star"}};
    const auto bytes = BinaryCodec::encode(schema, SchemaValue{obj});
    const auto decoded = BinaryCodec::decode(schema, bytes);
    REQUIRE(decoded.is_object());
    REQUIRE_EQ(std::get<int64_t>(decoded.as_object().at("id").as_scalar()), int64_t{7});
}

// 默认策略 missing=default：没有默认值的必填字段缺席时仍然报错（不能凭空造值）
TEST_CASE("decode rejects a missing required field without default under the default policy") {
    const auto schema = make_sample_v1();
    const auto bytes = BinaryCodec::encode(schema, SchemaValue{SchemaObject{}});

    auto threw = false;
    try {
        (void)BinaryCodec::decode(schema, bytes);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("apply_defaults fills optional fields that were omitted") {
    const auto schema = make_sample_v1();
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
    const auto schema = make_sample_v1();
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

// P0-6：长度前缀接近 SIZE_MAX 时 cur->pos + need 会回绕，使边界检查失效。
// 用例断言的是「必须抛 RuntimeError」这一行为。
// 注意：本用例**无法区分修复前后**——旧实现因 libc++ subspan 会把 count 截断到 0、
// 随后 cur->pos 回绕并在下一轮边界检查中抛同一异常而「碰巧安全」；
// 修复消除的是那条路径上的未定义行为（越界指针运算 + pos 回绕），而非可观测结果。
TEST_CASE("decode rejects a length prefix that would overflow the buffer") {
    const auto schema = make_sample_v1();
    // field 2（name, string）：tag = (2<<3)|2 = 0x12，随后是超长 varint 长度
    auto bytes = std::vector<uint8_t>{0x12};
    for (int i = 0; i < 9; ++i) {
        bytes.push_back(0xFF);
    }
    bytes.push_back(0x01);

    // 必须是「边界检查」抛出的 RuntimeError，而不是越界后由巨大分配偶然抛出的
    // length_error/bad_alloc——后者说明越界已经发生，只是碰巧没崩
    auto threw_runtime_error = false;
    try {
        (void)BinaryCodec::decode(schema, bytes);
    } catch (const star::excep::RuntimeError&) {
        threw_runtime_error = true;
    } catch (...) {
        threw_runtime_error = false;
    }
    REQUIRE(threw_runtime_error);
}

TEST_CASE("decode keeps unknown fields from a newer schema") {
    const auto schema_v2 = make_sample_v2();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    obj["name"] = SchemaValue{std::string{"legacy"}};
    obj["note"] = SchemaValue{std::string{"from the future"}};
    const auto bytes = BinaryCodec::encode(schema_v2, SchemaValue{obj});

    // 用旧 schema 解码：新增的 note 应被保留为 unknown field
    const auto schema_v1 = make_sample_v1();
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
    const auto schema_v2 = make_sample_v2();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    obj["name"] = SchemaValue{std::string{"legacy"}};
    obj["note"] = SchemaValue{std::string{"from the future"}};
    const auto original = BinaryCodec::encode(schema_v2, SchemaValue{obj});

    const auto schema_v1 = make_sample_v1();
    const auto first = BinaryCodec::decode_with_unknown(schema_v1, original);
    const auto reencoded = BinaryCodec::encode(schema_v1, first.value, first.unknown_fields);

    const auto second = BinaryCodec::decode_with_unknown(schema_v2, reencoded);
    REQUIRE_EQ(second.unknown_fields.size(), 0U);
    REQUIRE_EQ(std::get<std::string>(second.value.as_object().at("note").as_scalar()), std::string{"from the future"});
}

// 决策二/三：三类版本不一致各自有 per-channel 策略

TEST_CASE("unknown=drop discards fields from a newer schema") {
    const auto schema_v2 = make_sample_v2();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    obj["name"] = SchemaValue{std::string{"legacy"}};
    obj["note"] = SchemaValue{std::string{"from the future"}};
    const auto bytes = BinaryCodec::encode(schema_v2, SchemaValue{obj});

    auto policy = star::schema::DecodePolicy{};
    policy.unknown = star::schema::UnknownFieldPolicy::kDrop;
    const auto decoded = BinaryCodec::decode_with_unknown(make_sample_v1(), bytes, policy);

    REQUIRE_EQ(decoded.unknown_fields.size(), 0U);
    REQUIRE(decoded.value.as_object().count("note") == 0);
}

TEST_CASE("unknown=error rejects fields from a newer schema") {
    const auto schema_v2 = make_sample_v2();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    obj["name"] = SchemaValue{std::string{"legacy"}};
    obj["note"] = SchemaValue{std::string{"from the future"}};
    const auto bytes = BinaryCodec::encode(schema_v2, SchemaValue{obj});

    auto policy = star::schema::DecodePolicy{};
    policy.unknown = star::schema::UnknownFieldPolicy::kError;
    auto threw = false;
    try {
        (void)BinaryCodec::decode_with_unknown(make_sample_v1(), bytes, policy);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("missing=default fills a declared default for a field the producer did not send") {
    const auto schema_v1 = make_missing_v1();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{9}};
    const auto bytes = BinaryCodec::encode(schema_v1, SchemaValue{obj});

    const auto decoded = BinaryCodec::decode_with_unknown(make_missing_v2(), bytes);
    REQUIRE_EQ(std::get<std::string>(decoded.value.as_object().at("note").as_scalar()), std::string{"fallback"});
}

TEST_CASE("missing=error rejects a field that the producer did not send even if it has a default") {
    const auto schema_v1 = make_missing_v1();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{9}};
    const auto bytes = BinaryCodec::encode(schema_v1, SchemaValue{obj});

    auto policy = star::schema::DecodePolicy{};
    policy.missing = star::schema::MissingFieldPolicy::kError;
    auto threw = false;
    try {
        (void)BinaryCodec::decode_with_unknown(make_missing_v2(), bytes, policy);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("missing=default fills an empty array for an omitted repeated field") {
    // 旧生产者没有 tags 字段：required repeated 缺席等价于空数组，不应报错
    const auto schema = make_required_repeated();
    auto obj = SchemaObject{};
    obj["id"] = SchemaValue{int64_t{1}};
    const auto bytes = BinaryCodec::encode(schema, SchemaValue{obj});

    const auto decoded = BinaryCodec::decode_with_unknown(schema, bytes);
    const auto& tags = decoded.value.as_object().at("tags").as_array();
    REQUIRE(tags.empty());
}

TEST_CASE("type_mismatch=error rejects a field decoded with a different kind") {
    auto obj = SchemaObject{};
    obj["value"] = SchemaValue{std::string{"42"}};
    const auto bytes = BinaryCodec::encode(make_typed(2, star::schema::FieldKind::STRING), SchemaValue{obj});

    auto threw = false;
    try {
        (void)BinaryCodec::decode(make_typed(1, star::schema::FieldKind::INT64), bytes);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("type_mismatch=best_effort converts a numeric string to int") {
    auto obj = SchemaObject{};
    obj["value"] = SchemaValue{std::string{"42"}};
    const auto bytes = BinaryCodec::encode(make_typed(2, star::schema::FieldKind::STRING), SchemaValue{obj});

    auto policy = star::schema::DecodePolicy{};
    policy.type_mismatch = star::schema::TypeMismatchPolicy::kBestEffort;
    const auto decoded = BinaryCodec::decode(make_typed(1, star::schema::FieldKind::INT64), bytes, policy);
    REQUIRE_EQ(std::get<int64_t>(decoded.as_object().at("value").as_scalar()), int64_t{42});
}

TEST_CASE("type_mismatch=best_effort converts an int to string") {
    auto obj = SchemaObject{};
    obj["value"] = SchemaValue{int64_t{7}};
    const auto bytes = BinaryCodec::encode(make_typed(1, star::schema::FieldKind::INT64), SchemaValue{obj});

    auto policy = star::schema::DecodePolicy{};
    policy.type_mismatch = star::schema::TypeMismatchPolicy::kBestEffort;
    const auto decoded = BinaryCodec::decode(make_typed(2, star::schema::FieldKind::STRING), bytes, policy);
    REQUIRE_EQ(std::get<std::string>(decoded.as_object().at("value").as_scalar()), std::string{"7"});
}

TEST_CASE("type_mismatch=best_effort still rejects an unsafe conversion") {
    auto obj = SchemaObject{};
    obj["value"] = SchemaValue{std::string{"not-a-number"}};
    const auto bytes = BinaryCodec::encode(make_typed(2, star::schema::FieldKind::STRING), SchemaValue{obj});

    auto policy = star::schema::DecodePolicy{};
    policy.type_mismatch = star::schema::TypeMismatchPolicy::kBestEffort;
    auto threw = false;
    try {
        (void)BinaryCodec::decode(make_typed(1, star::schema::FieldKind::INT64), bytes, policy);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

}  // namespace

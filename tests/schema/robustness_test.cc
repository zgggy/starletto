#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include "basement/schema/binary_codec.h"
#include "basement/schema/decode_policy.h"
#include "basement/schema/schema_value.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::BinaryCodec;
using star::schema::DecodePolicy;

// 固定种子的 xorshift：用例可复现，不依赖平台随机源
struct Rng {
    uint64_t state{0x9E3779B97F4A7C15ULL};

    auto next() -> uint64_t {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }

    auto byte() -> uint8_t { return static_cast<uint8_t>(next() & 0xFFu); }

    auto bytes(size_t count) -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>(count);
        for (auto& item : out) {
            item = byte();
        }
        return out;
    }
};

auto make_field(uint32_t id, std::string name, star::schema::FieldKind kind) -> star::schema::FieldDescriptor {
    auto field = star::schema::FieldDescriptor{};
    field.id = id;
    field.name = std::move(name);
    field.kind = kind;
    return field;
}

auto load() -> star::schema::SchemaDescriptor {
    auto schema = star::schema::SchemaDescriptor{};
    schema.type_id = star::schema::SchemaTypeId{"test.robust", 1, 0};
    schema.root_name = "Robust";
    schema.fields.push_back(make_field(1, "id", star::schema::FieldKind::INT64));
    schema.fields.push_back(make_field(2, "name", star::schema::FieldKind::STRING));

    auto ratio = make_field(3, "ratio", star::schema::FieldKind::FLOAT64);
    ratio.optional = true;
    schema.fields.push_back(ratio);

    auto tags = make_field(4, "tags", star::schema::FieldKind::STRING);
    tags.optional = true;
    tags.repeated = true;
    schema.fields.push_back(tags);

    auto entries = make_field(5, "entries", star::schema::FieldKind::OBJECT);
    entries.optional = true;
    entries.has_value_kind = true;
    entries.value_kind = star::schema::FieldKind::OBJECT;
    entries.value_type_name = "Item";
    entries.value_children.push_back(make_field(1, "value", star::schema::FieldKind::INT64));
    auto note = make_field(2, "note", star::schema::FieldKind::STRING);
    note.has_default = true;
    note.default_value = star::schema::SchemaValue{std::string{"x"}};
    entries.value_children.push_back(note);
    schema.fields.push_back(entries);
    return schema;
}

auto fuzz_iterations(int fallback) -> int {
    if (const auto* value = std::getenv("STAR_FUZZ_ITERS"); value != nullptr) {
        try {
            return std::max(1, std::stoi(value));
        } catch (...) {
        }
    }
    return fallback;
}

auto severity_of(const DecodePolicy& policy) -> const char* {
    if (policy.type_mismatch == star::schema::TypeMismatchPolicy::kBestEffort) {
        return "best_effort";
    }
    return "error";
}

}  // namespace

// 随机字节不得让二进制解码崩溃/越界；失败必须是可捕获的异常。
// 该用例在 ASan/UBSan 下运行价值最大（见 docs/testing.md）。
TEST_CASE("binary decoder survives randomized input under every policy") {
    const auto schema = load();
    auto rng = Rng{};

    const auto policies = std::vector<DecodePolicy>{
        DecodePolicy{},
        DecodePolicy{star::schema::UnknownFieldPolicy::kDrop, star::schema::MissingFieldPolicy::kDefault,
                     star::schema::TypeMismatchPolicy::kBestEffort},
        DecodePolicy{star::schema::UnknownFieldPolicy::kError, star::schema::MissingFieldPolicy::kError,
                     star::schema::TypeMismatchPolicy::kError},
    };

    for (int iteration = 0; iteration < fuzz_iterations(1500); ++iteration) {
        const auto bytes = rng.bytes(rng.next() % 256);
        for (const auto& policy : policies) {
            try {
                const auto decoded = BinaryCodec::decode_with_unknown(schema, bytes, policy);
                REQUIRE(decoded.value.is_object());
            } catch (const std::exception&) {
                // 非法输入抛异常是预期行为
            }
        }
    }
    REQUIRE(severity_of(policies[1]) != nullptr);
}

// 文本解析器（conf/值文件）面对任意输入必须要么返回，要么抛异常。
TEST_CASE("schema value parser survives randomized text") {
    auto rng = Rng{};
    for (int iteration = 0; iteration < fuzz_iterations(2000); ++iteration) {
        const auto bytes = rng.bytes(rng.next() % 128);
        const auto text = std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        try {
            const auto parsed = star::schema::SchemaParser{text}.parse();
            (void)parsed;
        } catch (const std::exception&) {
        }
    }
}

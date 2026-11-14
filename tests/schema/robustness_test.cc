#include <cstdint>
#include <cstdlib>
#include <span>
#include <string>
#include <vector>

#include "basement/schema/binary_codec.h"
#include "basement/schema/decode_policy.h"
#include "basement/schema/native/native_schema_loader.h"
#include "basement/schema/schema_value.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::BinaryCodec;
using star::schema::DecodePolicy;
using star::schema::NativeSchemaLoader;

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

constexpr const char* kSchemaText = R"SCHEMA(
schema test.robust
version 1
root Robust {
    1 id : int
    2 name : string
    3 ratio : float ?
    4 tags : string[] ?
    5 entries : {string : Item} ?
}
def Item {
    1 value : int
    2 note : string = "x"
}
)SCHEMA";

auto load() -> star::schema::SchemaDescriptor {
    return NativeSchemaLoader{}.load_schema_text(kSchemaText);
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

// 原生 schema loader 面对任意文本同样只能失败，不能崩溃
TEST_CASE("native schema loader survives randomized text") {
    auto rng = Rng{};
    for (int iteration = 0; iteration < fuzz_iterations(800); ++iteration) {
        const auto bytes = rng.bytes(rng.next() % 256);
        const auto text = std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        try {
            (void)NativeSchemaLoader{}.load_schema_text(text);
        } catch (const std::exception&) {
        }
    }
}

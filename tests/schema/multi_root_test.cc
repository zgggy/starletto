#include <string>
#include <vector>

#include "multi_root.s.h"

#include "basement/schema/schema_registry.h"

#include "tests/support/star_test.h"

namespace {

using Alpha = star::test::multi_root::Alpha;
using Beta = star::test::multi_root::Beta;

}  // namespace

// P2-4：一个 .schema 可以有多个 root；每个 root 必须有独立的类型身份，
// 否则同文件里的两个消息类型会在 registry 里互相覆盖
TEST_CASE("multi-root schema gives each root its own identity") {
    const auto alpha_id = Alpha::schema_type_id();
    const auto beta_id = Beta::schema_type_id();

    REQUIRE_EQ(alpha_id.name, std::string("test.multi_root.Alpha"));
    REQUIRE_EQ(beta_id.name, std::string("test.multi_root.Beta"));
    REQUIRE_EQ(alpha_id.version, int64_t{1});
    REQUIRE_EQ(beta_id.version, int64_t{1});
    REQUIRE(alpha_id.hash != 0);
    REQUIRE(beta_id.hash != 0);
    // 两个 root 的字段不同，内容哈希也必须不同
    REQUIRE(alpha_id.hash != beta_id.hash);

    auto registry = star::schema::SchemaRegistry{};
    Alpha::register_schema(registry);
    Beta::register_schema(registry);
    REQUIRE_EQ(registry.size(), 2U);
    REQUIRE(registry.lookup(alpha_id).status == star::schema::SchemaMatchStatus::kExact);
    REQUIRE(registry.lookup(beta_id).status == star::schema::SchemaMatchStatus::kExact);
}

TEST_CASE("multi-root messages roundtrip independently") {
    auto alpha = Alpha{};
    alpha.a = 42;
    alpha.label = std::string("alpha");
    const auto alpha_back = Alpha::from_binary(alpha.encode_binary());
    REQUIRE_EQ(alpha_back.a, int64_t{42});
    REQUIRE_EQ(alpha_back.label.value_or(""), std::string("alpha"));

    auto beta = Beta{};
    beta.b = std::string("beta");
    beta.values = std::vector<int64_t>{1, 2, 3};
    const auto beta_back = Beta::from_binary(beta.encode_binary());
    REQUIRE_EQ(beta_back.b, std::string("beta"));
    REQUIRE_EQ(beta_back.values->size(), 3U);
}

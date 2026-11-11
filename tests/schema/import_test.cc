#include <string>
#include <vector>

#include "tests/schema/imports/uses.s.h"

#include "basement/schema/schema_registry.h"

#include "tests/support/star_test.h"

namespace {

using Point = star::test::import_base::Point;
using Shape = star::test::import_uses::Shape;

}  // namespace

// P2-4：跨文件 import——uses.schema 引用 base.schema 的 Point，
// 生成头自动 include 依赖头，descriptor children 也跨文件解析
TEST_CASE("imported types are usable as fields") {
    auto shape = Shape{};
    shape.name = std::string("poly");
    shape.origin = Point{};
    shape.origin.x = 1;
    shape.origin.y = 2;
    shape.points = std::vector<Point>{};
    shape.points->push_back(Point{3, 4});

    // TLV 往返：嵌套结构必须靠跨文件 descriptor children 才能正确解码
    const auto bytes = shape.encode_binary();
    const auto back = Shape::from_binary(bytes);
    REQUIRE_EQ(back.name, std::string("poly"));
    REQUIRE_EQ(back.origin.x, int64_t{1});
    REQUIRE_EQ(back.origin.y, int64_t{2});
    REQUIRE_EQ(back.points->size(), 1U);
    REQUIRE_EQ(back.points->front().x, int64_t{3});

    // 固定布局往返：跨文件类型走限定名 encode/from_fixed_layout
    const auto fixed = shape.encode_fixed_layout();
    const auto fixed_back = Shape::from_fixed_layout(fixed);
    REQUIRE_EQ(fixed_back.name, std::string("poly"));
    REQUIRE_EQ(fixed_back.origin.x, int64_t{1});

    auto registry = star::schema::SchemaRegistry{};
    Shape::register_schema(registry);
    Point::register_schema(registry);
    REQUIRE_EQ(registry.size(), 2U);
    REQUIRE_EQ(Shape::schema_type_id().name, std::string("test.import_uses"));
    REQUIRE_EQ(Point::schema_type_id().name, std::string("test.import_base"));
}

TEST_CASE("imported types can appear in maps") {
    auto shape = Shape{};
    shape.name = std::string("map");
    shape.origin = Point{};
    shape.labels = std::map<std::string, Point>{};
    (*shape.labels)["a"] = Point{7, 8};

    const auto back = Shape::from_binary(shape.encode_binary());
    REQUIRE_EQ(back.labels->at("a").y, int64_t{8});
}

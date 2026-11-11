#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "basement/schema/builtin/runtime/process_plan.s.h"
#include "basement/schema/fixed_layout_codec.h"

#include "tests/support/star_test.h"

namespace {

using Plan = star::generated::runtime::process_plan::ProcessPlan;
using ProcessGroupPlan = star::generated::runtime::process_plan::ProcessGroupPlan;

auto make_plan() -> Plan {
    auto group = ProcessGroupPlan{};
    group.name = "setting";
    group.component_names = std::vector<std::string>{"parameter"};

    auto plan = Plan{};
    plan.groups = std::vector<ProcessGroupPlan>{group};
    return plan;
}

}  // namespace

// 固定布局是位置语义：帧头必须能自证身份，否则读取方无法判断「这是不是我的类型」
TEST_CASE("fixed layout frames carry the schema identity") {
    const auto plan = make_plan();
    const auto bytes = plan.encode_fixed_layout();

    REQUIRE(star::schema::is_fixed_layout_frame(bytes));
    const auto view = star::schema::FixedLayoutView(bytes.data(), bytes.size());
    const auto id = Plan::schema_type_id();
    REQUIRE(view.matches(id));
    REQUIRE_EQ(view.schema_version(), id.version);
    REQUIRE_EQ(view.schema_hash(), id.hash);
}

TEST_CASE("fixed layout roundtrips a generated message") {
    const auto plan = make_plan();
    const auto bytes = plan.encode_fixed_layout();

    const auto back = Plan::from_fixed_layout(bytes);
    REQUIRE(back.groups.has_value());
    REQUIRE_EQ(back.groups->size(), 1U);
    REQUIRE_EQ(back.groups->front().name, std::string("setting"));
    REQUIRE(back.groups->front().component_names.has_value());
    REQUIRE_EQ(back.groups->front().component_names->size(), 1U);
    REQUIRE_EQ(back.groups->front().component_names->front(), std::string("parameter"));
}

// 跨版本帧必须被拒绝：位置布局没有字段 id，无法跳过/补默认值
TEST_CASE("fixed layout rejects a frame from a different schema version") {
    auto bytes = make_plan().encode_fixed_layout();
    auto* header = reinterpret_cast<star::schema::FixedLayoutHeader*>(bytes.data());
    header->schema_hash = header->schema_hash == 0 ? 1 : header->schema_hash + 1;

    auto threw = false;
    try {
        (void)Plan::from_fixed_layout(std::span<const uint8_t>(bytes.data(), bytes.size()));
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

// 二进制解码器不能误吃固定布局帧（此前读路径固定调用 from_binary，跨进程消息必然解错）
TEST_CASE("binary decoder does not accept a fixed layout frame") {
    const auto bytes = make_plan().encode_fixed_layout();
    auto threw = false;
    try {
        (void)Plan::from_binary(bytes);
    } catch (const std::exception&) {
        threw = true;
    }
    REQUIRE(threw);
}

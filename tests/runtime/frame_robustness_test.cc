#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/fixed_layout_codec.h"

#include "tests/support/star_test.h"

namespace {

using Plan = star::generated::runtime::component_load_plan::ComponentLoadPlan;

struct Rng {
    uint64_t state{0xD1B54A32D192ED03ULL};

    auto next() -> uint64_t {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return state;
    }
};

auto fuzz_iterations(int fallback) -> int {
    if (const auto* value = std::getenv("STAR_FUZZ_ITERS"); value != nullptr) {
        try {
            return std::max(1, std::stoi(value));
        } catch (...) {
        }
    }
    return fallback;
}

auto make_valid_frame() -> std::vector<uint8_t> {
    auto plan = Plan{};
    plan.module_name = "module";
    plan.component_name = "component";
    plan.component_class = "test::Component";
    return plan.encode_fixed_layout();
}

}  // namespace

// 固定布局帧解析面对随机/变异字节不能越界；异常可捕获即可。
TEST_CASE("fixed layout decoder survives mutated frames") {
    const auto valid = make_valid_frame();
    REQUIRE(star::schema::is_fixed_layout_frame(valid));

    auto rng = Rng{};
    for (int iteration = 0; iteration < fuzz_iterations(1000); ++iteration) {
        auto frame = valid;
        const auto mutations = 1 + (rng.next() % 8);
        for (uint64_t i = 0; i < mutations; ++i) {
            frame[rng.next() % frame.size()] ^= static_cast<uint8_t>(rng.next() & 0xFFu);
        }
        try {
            if (star::schema::is_fixed_layout_frame(frame)) {
                const auto decoded = Plan::from_fixed_layout(frame);
                (void)decoded;
            }
        } catch (const std::exception&) {
        }
    }
}

TEST_CASE("fixed layout decoder survives fully random frames") {
    auto rng = Rng{};
    for (int iteration = 0; iteration < fuzz_iterations(1000); ++iteration) {
        auto frame = std::vector<uint8_t>(rng.next() % 512);
        for (auto& byte : frame) {
            byte = static_cast<uint8_t>(rng.next() & 0xFFu);
        }
        try {
            if (star::schema::is_fixed_layout_frame(frame)) {
                const auto decoded = Plan::from_fixed_layout(frame);
                (void)decoded;
            }
        } catch (const std::exception&) {
        }
    }
}

// ASan 回归：fixed_section_size 与 total_size 不一致、变长偏移越界都必须在
// 解码前被拒绝，不能靠 size_ - fixed_size 无符号回绕「通过」检查。
TEST_CASE("fixed layout rejects inconsistent section sizes and out-of-range refs") {
    const auto make_header = [](const std::vector<uint8_t>& frame) {
        auto header = star::schema::FixedLayoutHeader{};
        std::memcpy(&header, frame.data(), sizeof(header));
        return header;
    };

    auto frame = make_valid_frame();
    auto header = make_header(frame);
    header.total_size = static_cast<uint32_t>(sizeof(star::schema::FixedLayoutHeader));
    header.fixed_section_size = static_cast<uint16_t>(frame.size() + 16);
    std::memcpy(frame.data(), &header, sizeof(header));
    REQUIRE(not star::schema::is_fixed_layout_frame(frame));
    try {
        (void)Plan::from_fixed_layout(frame);
    } catch (const std::exception&) {
    }

    frame = make_valid_frame();
    header = make_header(frame);
    header.fixed_section_size = static_cast<uint16_t>(sizeof(star::schema::FixedLayoutHeader) + 1);
    std::memcpy(frame.data(), &header, sizeof(header));
    REQUIRE(not star::schema::is_fixed_layout_frame(frame));
    try {
        (void)Plan::from_fixed_layout(frame);
    } catch (const std::exception&) {
    }

    // 帧头合法但第一个槽位的字符串引用指向帧外
    frame = make_valid_frame();
    auto slot = star::schema::FixedFieldSlot{};
    std::memcpy(&slot, frame.data() + sizeof(star::schema::FixedLayoutHeader), sizeof(slot));
    slot.type_tag = star::schema::FixedFieldSlot::kTagString;
    slot.data.var_ref.offset = 0xFFFFFF00U;
    slot.data.var_ref.length = 0x100U;
    std::memcpy(frame.data() + sizeof(star::schema::FixedLayoutHeader), &slot, sizeof(slot));
    REQUIRE(star::schema::is_fixed_layout_frame(frame));
    try {
        (void)Plan::from_fixed_layout(frame);
    } catch (const std::exception&) {
    }
}

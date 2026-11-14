#include <cstdint>
#include <cstdlib>
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

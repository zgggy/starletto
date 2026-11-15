#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "basement/runtime/component/frame_inputs.h"
#include "basement/runtime/component/typed_component.h"
#include "basement/transport/api/message_bus.h"

#include "tests/support/star_test.h"

namespace {

struct TestMsg {
    int value{0};

    auto encode_binary() const -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>(sizeof(int));
        std::memcpy(out.data(), &value, sizeof(int));
        return out;
    }

    static auto from_binary(std::span<const uint8_t> bytes) -> TestMsg {
        auto msg = TestMsg{};
        if (bytes.size() >= sizeof(int)) {
            std::memcpy(&msg.value, bytes.data(), sizeof(int));
        }
        return msg;
    }
};

class TypedProbe final
    : public star::runtime::TypedComponent<TypedProbe,
          star::runtime::TriggerInput<TestMsg, "left">,
          star::runtime::TriggerInput<TestMsg, "right">> {
  public:
    auto process_once(const auto& left, const auto& right) -> bool {
        seen_left = left.value;
        seen_right = right.value;
        return true;
    }

    int seen_left{-1};
    int seen_right{-1};
};

auto make_message(int value) -> star::transport::MessageView {
    auto owner = std::make_shared<std::vector<uint8_t>>(sizeof(int));
    std::memcpy(owner->data(), &value, sizeof(int));
    auto view = star::transport::MessageView{};
    view.payload = star::base::TypedConstPtr::from(
        std::make_shared<star::transport::BinaryPayloadView>(star::transport::BinaryPayloadView{owner, owner->data(), owner->size()}));
    return view;
}

}  // namespace

TEST_CASE("TypedComponent adapts FrameInputs into component-specific typed inputs") {
    auto probe = TypedProbe{};

    auto raw = star::runtime::FrameInputs{};
    raw.frame_id = 7;
    raw.messages.emplace("left", make_message(11));
    raw.messages.emplace("right", make_message(22));

    REQUIRE_EQ(probe.declared_trigger_ports().size(), 2U);
    REQUIRE_EQ(probe.declared_trigger_ports()[0], std::string_view("left"));
    REQUIRE_EQ(probe.declared_trigger_ports()[1], std::string_view("right"));
    REQUIRE(static_cast<star::runtime::Component&>(probe).process_once(raw));
    REQUIRE_EQ(probe.seen_left, 11);
    REQUIRE_EQ(probe.seen_right, 22);
}

TEST_CASE("TypedComponent rejects a missing trigger message") {
    auto probe = TypedProbe{};
    auto raw = star::runtime::FrameInputs{};
    raw.messages.emplace("left", make_message(11));

    auto threw = false;
    try {
        (void)static_cast<star::runtime::Component&>(probe).process_once(raw);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
    REQUIRE_EQ(probe.seen_left, -1);
    REQUIRE_EQ(probe.seen_right, -1);
}

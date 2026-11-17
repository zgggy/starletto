#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "basement/runtime/component/component.h"
#include "basement/schema/fixed_layout_codec.h"
#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_views.h"

#include "tests/schema/wire/v1.s.h"
#include "tests/schema/wire/v2.s.h"

#include "tests/support/star_test.h"

namespace {

using V1Msg = star::test::wire::v1::Msg;
using V2Msg = star::test::wire::v2::Msg;
using star::base::TypedConstPtr;
using star::schema::SchemaTypeId;
using star::transport::ChannelDescriptor;
using star::transport::MessageSerializer;
using star::transport::MessageView;
using star::transport::SerializedMessage;
using star::transport::ShmMessageBus;
using star::transport::ShmSlotView;

struct Captured {
    mutable std::mutex mutex;
    std::vector<uint8_t> payload;
    SchemaTypeId sender;
    int count{0};
};

auto make_descriptor(const std::string& name, const SchemaTypeId& id) -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = name;
    descriptor.type_name = id.name;
    descriptor.schema_type_id = id;
    return descriptor;
}

template <typename _Msg>
auto make_binary_serialized(const std::shared_ptr<const _Msg>& msg) -> SerializedMessage {
    auto serialized = SerializedMessage{};
    serialized.view.payload = TypedConstPtr::from(msg);
    serialized.view.schema_type_id = msg->schema_type_id();
    serialized.serializer.payload_size_hint = 0;
    serialized.serializer.encode_binary = [msg]() { return msg->encode_binary(); };
    return serialized;
}

template <typename _Msg>
auto make_fixed_serialized(const std::shared_ptr<const _Msg>& msg) -> SerializedMessage {
    auto serialized = SerializedMessage{};
    serialized.view.payload = TypedConstPtr::from(msg);
    serialized.view.schema_type_id = msg->schema_type_id();
    serialized.serializer.payload_size_hint = 0;
    serialized.serializer.encode_into = [msg](uint8_t* buf, size_t cap) -> size_t {
        return msg->encode_fixed_layout_into(buf, cap);
    };
    return serialized;
}

auto wait_for(const Captured& captured, int expected = 1) -> bool {
    for (int i = 0; i < 200; ++i) {
        {
            std::lock_guard<std::mutex> lock(captured.mutex);
            if (captured.count >= expected) {
                return true;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return false;
}

auto make_v1(int64_t id, std::string note) -> V1Msg {
    auto msg = V1Msg{};
    msg.id = id;
    msg.note = std::move(note);
    return msg;
}

auto make_v2(int64_t id, std::string note, std::optional<std::string> extra) -> V2Msg {
    auto msg = V2Msg{};
    msg.id = id;
    msg.note = std::move(note);
    msg.extra = std::move(extra);
    return msg;
}

}  // namespace

// P0.1：shm 帧携带发送方 schema 身份；读侧能看到 v2，而本地编译的是 v1
TEST_CASE("shm frames carry the sender schema identity") {
    const auto bus = std::make_shared<ShmMessageBus>();
    const auto channel = "/test/wire/identity";
    auto captured = Captured{};
    const auto reader = bus->create_reader(make_descriptor(channel, V1Msg::schema_type_id()), [&](const MessageView& message) {
        const auto slot = message.payload.as<ShmSlotView>();
        if (not slot) {
            return;
        }
        std::lock_guard<std::mutex> lock(captured.mutex);
        const auto bytes = slot->bytes();
        captured.payload.assign(bytes.begin(), bytes.end());
        captured.sender = message.schema_type_id;
        ++captured.count;
    });
    const auto writer = bus->create_writer(make_descriptor(channel, V2Msg::schema_type_id()));

    const auto v2 = std::make_shared<const V2Msg>(make_v2(7, "hello", std::string("future")));
    REQUIRE(writer->write_serialized(make_binary_serialized(v2)).ok());
    REQUIRE(wait_for(captured));

    std::vector<uint8_t> payload;
    SchemaTypeId sender;
    {
        std::lock_guard<std::mutex> lock(captured.mutex);
        payload = captured.payload;
        sender = captured.sender;
    }
    REQUIRE_EQ(sender.name, std::string("test.wire.evolve"));
    REQUIRE_EQ(sender.version, int64_t{2});

    // v1 读 v2：新增字段进 unknown，缺失的可选字段保持 null
    const auto v1 = star::runtime::Component::decode_payload<V1Msg>(payload, star::schema::DecodePolicy{}, sender);
    REQUIRE_EQ(v1.id, int64_t{7});
    REQUIRE_EQ(v1.note, std::string("hello"));
    REQUIRE_EQ(v1.unknown_fields().size(), 1U);
}

TEST_CASE("wire identity supports the reverse direction v1 sender to v2 receiver") {
    const auto bus = std::make_shared<ShmMessageBus>();
    const auto channel = "/test/wire/identity/reverse";
    auto captured = Captured{};
    const auto reader = bus->create_reader(make_descriptor(channel, V2Msg::schema_type_id()), [&](const MessageView& message) {
        const auto slot = message.payload.as<ShmSlotView>();
        if (not slot) {
            return;
        }
        std::lock_guard<std::mutex> lock(captured.mutex);
        const auto bytes = slot->bytes();
        captured.payload.assign(bytes.begin(), bytes.end());
        captured.sender = message.schema_type_id;
        ++captured.count;
    });
    const auto writer = bus->create_writer(make_descriptor(channel, V1Msg::schema_type_id()));

    const auto v1 = std::make_shared<const V1Msg>(make_v1(9, "old"));
    REQUIRE(writer->write_serialized(make_binary_serialized(v1)).ok());
    REQUIRE(wait_for(captured));

    std::vector<uint8_t> payload;
    SchemaTypeId sender;
    {
        std::lock_guard<std::mutex> lock(captured.mutex);
        payload = captured.payload;
        sender = captured.sender;
    }
    const auto v2 = star::runtime::Component::decode_payload<V2Msg>(payload, star::schema::DecodePolicy{}, sender);
    REQUIRE_EQ(v2.id, int64_t{9});
    REQUIRE_EQ(v2.note, std::string("old"));
    REQUIRE(not v2.extra.has_value());
}

// 身份校验：name 不同任何策略都不能兜底；同版本 hash 不同按 type_mismatch 策略
TEST_CASE("wire identity rejects a name mismatch and a same-version hash mismatch") {
    const auto bytes = make_v1(1, "x").encode_binary();

    auto threw_name = false;
    try {
        (void)star::runtime::Component::decode_payload<V1Msg>(
            bytes, star::schema::DecodePolicy{}, SchemaTypeId{"other.schema", 1, 0});
    } catch (const star::excep::RuntimeError&) {
        threw_name = true;
    }
    REQUIRE(threw_name);

    auto threw_hash = false;
    try {
        (void)star::runtime::Component::decode_payload<V1Msg>(
            bytes, star::schema::DecodePolicy{}, SchemaTypeId{"test.wire.evolve", 1, 0xDEADBEEF});
    } catch (const star::excep::RuntimeError&) {
        threw_hash = true;
    }
    REQUIRE(threw_hash);

    auto policy = star::schema::DecodePolicy{};
    policy.type_mismatch = star::schema::TypeMismatchPolicy::kBestEffort;
    const auto decoded = star::runtime::Component::decode_payload<V1Msg>(
        bytes, policy, SchemaTypeId{"test.wire.evolve", 1, 0xDEADBEEF});
    REQUIRE_EQ(decoded.id, int64_t{1});
}

// 固定布局经外层帧传输：payload 仍是 fixed-layout 帧，身份来自外层帧
TEST_CASE("fixed layout frames keep their identity through the shm frame header") {
    const auto bus = std::make_shared<ShmMessageBus>();
    const auto channel = "/test/wire/identity/fixed";
    auto captured = Captured{};
    const auto reader = bus->create_reader(make_descriptor(channel, V1Msg::schema_type_id()), [&](const MessageView& message) {
        const auto slot = message.payload.as<ShmSlotView>();
        if (not slot) {
            return;
        }
        std::lock_guard<std::mutex> lock(captured.mutex);
        const auto bytes = slot->bytes();
        captured.payload.assign(bytes.begin(), bytes.end());
        captured.sender = message.schema_type_id;
        ++captured.count;
    });
    const auto writer = bus->create_writer(make_descriptor(channel, V1Msg::schema_type_id()));

    const auto msg = std::make_shared<const V1Msg>(make_v1(3, "fixed"));
    REQUIRE(writer->write_serialized(make_fixed_serialized(msg)).ok());
    REQUIRE(wait_for(captured));

    std::vector<uint8_t> payload;
    SchemaTypeId sender;
    {
        std::lock_guard<std::mutex> lock(captured.mutex);
        payload = captured.payload;
        sender = captured.sender;
    }
    REQUIRE(star::schema::is_fixed_layout_frame(payload));
    REQUIRE_EQ(sender.version, int64_t{1});
    const auto decoded = star::runtime::Component::decode_payload<V1Msg>(payload, star::schema::DecodePolicy{}, sender);
    REQUIRE_EQ(decoded.id, int64_t{3});
    REQUIRE_EQ(decoded.note, std::string("fixed"));
}

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "basement/transport/native/native_message_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::schema::SchemaTypeId;
using star::transport::ChannelDescriptor;
using star::transport::ChannelOptions;
using star::transport::DropPolicy;
using star::transport::MessageSerializer;
using star::transport::MessageView;
using star::transport::NativeMessageBus;
using star::transport::Reader;
using star::transport::SerializedMessage;
using star::transport::Writer;
using star::transport::WriteStatus;

struct TestMsg {
    int value{0};
};

auto make_type_id() -> SchemaTypeId { return SchemaTypeId{"test.Msg", 1, 0}; }

auto make_descriptor(const std::string& name, uint32_t depth = 0) -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = name;
    descriptor.type_name = "test.Msg";
    descriptor.schema_type_id = make_type_id();
    descriptor.options = ChannelOptions{depth, DropPolicy::KEEP_LAST, false};
    return descriptor;
}

auto make_message(int value) -> MessageView {
    auto view = MessageView{};
    view.payload = TypedConstPtr::from(std::make_shared<const TestMsg>(TestMsg{value}));
    view.schema_type_id = make_type_id();
    return view;
}

// NativeMessageBus 依赖 enable_shared_from_this，必须由 shared_ptr 持有；
// 且 create_reader 返回的 Reader 必须保活（析构时会注销回调）
auto make_bus() -> std::shared_ptr<NativeMessageBus> { return std::make_shared<NativeMessageBus>(); }

TEST_CASE("declare_channel rejects empty channel name") {
    const auto bus = make_bus();
    auto threw = false;
    try {
        bus->declare_channel(make_descriptor(""));
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("declare_channel rejects a type mismatch on the same channel") {
    const auto bus = make_bus();
    bus->declare_channel(make_descriptor("chan"));

    auto other = make_descriptor("chan");
    other.type_name = "other.Type";
    auto threw = false;
    try {
        bus->declare_channel(other);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("declare_channel is idempotent for a matching descriptor") {
    const auto bus = make_bus();
    bus->declare_channel(make_descriptor("chan"));
    bus->declare_channel(make_descriptor("chan"));
    REQUIRE_EQ(bus->declared_channels().size(), 1U);
}

TEST_CASE("write delivers the typed payload to a reader callback") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan");

    auto received = std::vector<int>{};
    const auto reader = bus->create_reader(descriptor, [&](const MessageView& message) {
        const auto payload = message.payload.as<TestMsg>();
        REQUIRE(payload != nullptr);
        received.push_back(payload->value);
    });

    const auto writer = bus->create_writer(descriptor);
    const auto result = writer->write(make_message(7));

    REQUIRE(result.ok());
    REQUIRE_EQ(received.size(), 1U);
    REQUIRE_EQ(received.front(), 7);
}

TEST_CASE("write delivers to every registered reader") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan");

    auto first = 0;
    auto second = 0;
    const auto reader_a = bus->create_reader(descriptor, [&](const MessageView& message) { first = message.payload.as<TestMsg>()->value; });
    const auto reader_b = bus->create_reader(descriptor, [&](const MessageView& message) { second = message.payload.as<TestMsg>()->value; });

    const auto writer = bus->create_writer(descriptor);
    writer->write(make_message(11));

    REQUIRE_EQ(first, 11);
    REQUIRE_EQ(second, 11);
}

TEST_CASE("peek_latest returns the most recent message") {
    const auto bus = make_bus();
    // 历史仅在 depth > 0 时保留，peek_latest 依赖 reader_history
    const auto descriptor = make_descriptor("chan", 2);
    const auto reader = bus->create_reader(descriptor, [](const MessageView&) {});

    const auto writer = bus->create_writer(descriptor);
    writer->write(make_message(1));
    writer->write(make_message(2));

    const auto latest = bus->peek_latest("chan");
    REQUIRE(latest.has_value());
    REQUIRE_EQ(latest->payload.as<TestMsg>()->value, 2);
}

TEST_CASE("peek_latest is empty when the channel keeps no history") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan", 0);
    const auto reader = bus->create_reader(descriptor, [](const MessageView&) {});

    const auto writer = bus->create_writer(descriptor);
    writer->write(make_message(1));

    REQUIRE(not bus->peek_latest("chan").has_value());
}

TEST_CASE("peek_latest is empty for an unknown channel") {
    const auto bus = make_bus();
    REQUIRE(not bus->peek_latest("nope").has_value());
}

TEST_CASE("reader history respects the configured channel depth") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan", 2);
    const auto reader = bus->create_reader(descriptor, [](const MessageView&) {});

    const auto writer = bus->create_writer(descriptor);
    for (int i = 0; i < 5; ++i) {
        writer->write(make_message(i));
    }

    REQUIRE_EQ(bus->reader_history_size("chan"), 2U);
    REQUIRE_EQ(bus->peek_latest("chan")->payload.as<TestMsg>()->value, 4);
}

TEST_CASE("write records channel statistics") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan");

    REQUIRE(not bus->channel_stats("chan").has_value());

    const auto reader = bus->create_reader(descriptor, [](const MessageView&) {});
    const auto writer = bus->create_writer(descriptor);
    writer->write(make_message(1));
    writer->write(make_message(2));

    const auto snapshot = bus->channel_stats("chan");
    REQUIRE(snapshot.has_value());
    REQUIRE_EQ(snapshot->channel_name, std::string("chan"));
    REQUIRE_EQ(snapshot->write_count, 2ULL);
    REQUIRE_EQ(snapshot->deliver_count, 2ULL);
    REQUIRE_EQ(snapshot->dropped_count, 0ULL);

    const auto all = bus->all_channel_stats();
    REQUIRE_EQ(all.size(), 1U);
}

TEST_CASE("write_serialized accounts for the payload size hint") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan");
    const auto reader = bus->create_reader(descriptor, [](const MessageView&) {});

    auto message = SerializedMessage{};
    message.view = make_message(1);
    message.serializer = MessageSerializer{};
    message.serializer.payload_size_hint = 128;
    message.serializer.encode_binary = []() { return std::vector<uint8_t>{0x01, 0x02}; };

    const auto writer = bus->create_writer(descriptor);
    const auto result = writer->write_serialized(message);

    REQUIRE(result.ok());
    const auto snapshot = bus->channel_stats("chan");
    REQUIRE(snapshot.has_value());
    REQUIRE_EQ(snapshot->payload_bytes_total, 128ULL);
    REQUIRE_EQ(snapshot->payload_bytes_last, 128ULL);
}

// P0-3 回归：回调抛异常不得杀死进程，也不得中断后续投递；
// 失败必须计数可见（callback_failures）
TEST_CASE("a throwing reader callback does not abort delivery or the process") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan");

    auto received = 0;
    const auto bad = bus->create_reader(descriptor, [](const MessageView&) { throw std::runtime_error("boom"); });
    const auto good = bus->create_reader(descriptor, [&](const MessageView&) { received += 1; });

    const auto writer = bus->create_writer(descriptor);
    REQUIRE(writer->write(make_message(1)).ok());
    REQUIRE(writer->write(make_message(2)).ok());

    REQUIRE_EQ(received, 2);
    const auto stats = bus->channel_stats("chan");
    REQUIRE(stats.has_value());
    REQUIRE_EQ(stats->callback_failures, 2ULL);
}

// P0-5 回归：DROP_NEWEST 没有消费/背压信号，无法正确实现——声明时明确拒绝，
// 而不是接受配置后在队列填满时永久丢弃所有新消息
TEST_CASE("declare_channel rejects DROP_NEWEST as unimplemented") {
    const auto bus = make_bus();
    auto descriptor = make_descriptor("chan");
    descriptor.options = ChannelOptions{1, DropPolicy::DROP_NEWEST, false};

    auto threw = false;
    try {
        bus->declare_channel(descriptor);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

TEST_CASE("write on a mismatched schema type reports EXCEPTION") {
    const auto bus = make_bus();
    const auto descriptor = make_descriptor("chan");
    const auto reader = bus->create_reader(descriptor, [](const MessageView&) {});

    auto bad = make_message(1);
    bad.schema_type_id = SchemaTypeId{"other.Msg", 1, 0};

    const auto writer = bus->create_writer(descriptor);
    // Writer 捕获异常并转成状态码，而不是向调用方抛出
    const auto result = writer->write(bad);

    REQUIRE(not result.ok());
    REQUIRE(result.status == WriteStatus::EXCEPTION);
    REQUIRE(not result.error_message.empty());
}

}  // namespace

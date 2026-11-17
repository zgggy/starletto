#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "basement/transport/record/message_publisher.h"
#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_routed_message_bus.h"
#include "basement/transport/shm/shm_views.h"

#include "tests/support/star_test.h"

namespace {

using star::transport::ChannelDescriptor;
using star::transport::MessagePublisher;
using star::transport::MessageReplayer;
using star::transport::MessageView;
using star::transport::ShmMessageBus;
using star::transport::ShmRoutedMessageBus;
using star::transport::ShmSlotView;

auto make_entry(const std::string& channel, const std::string& type_name, star::schema::SchemaTypeId id, std::vector<uint8_t> payload)
    -> MessageReplayer::RecordEntry {
    auto entry = MessageReplayer::RecordEntry{};
    entry.channel_name = channel;
    entry.type_name = type_name;
    entry.schema_type_id = std::move(id);
    entry.payload = std::move(payload);
    return entry;
}

}  // namespace

// 回放发布：字节原样写回，schema 身份随条目带过去（接收端策略据此生效）
TEST_CASE("MessagePublisher writes recorded bytes back to a channel") {
    auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = ChannelDescriptor{};
    descriptor.name = "/test/replay/publish/native";
    descriptor.type_name = "test.Msg";
    descriptor.schema_type_id = star::schema::SchemaTypeId{"test.replay.msg", 2, 0xABCD};

    auto mutex = std::mutex{};
    auto received = std::vector<MessageView>{};
    const auto reader = bus->create_reader(descriptor, [&](const MessageView& message) {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(message);
    });
    auto publisher = MessagePublisher{*bus};

    // 连续写入会被环形缓冲合并（读侧只看到最新一帧），逐帧等待后再发下一帧
    REQUIRE(publisher.publish(make_entry(descriptor.name, descriptor.type_name, descriptor.schema_type_id, {0x01, 0x02})).ok());
    for (int i = 0; i < 100 && received.size() < 1; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(publisher.publish(make_entry(descriptor.name, descriptor.type_name, descriptor.schema_type_id, {0x03})).ok());
    for (int i = 0; i < 100 && received.size() < 2; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    std::lock_guard<std::mutex> lock(mutex);
    REQUIRE_EQ(received.size(), 2U);
    REQUIRE_EQ(received[0].schema_type_id.name, std::string("test.replay.msg"));
    REQUIRE_EQ(received[0].schema_type_id.version, int64_t{2});
    const auto slot = received[0].payload.as<ShmSlotView>();
    REQUIRE(slot != nullptr);
    REQUIRE_EQ(std::vector<uint8_t>(slot->data, slot->data + slot->size), (std::vector<uint8_t>{0x01, 0x02}));
}

// CLI 的发布路径：routed 总线按通道集合写 shm，另一个总线按订阅集合读
TEST_CASE("MessagePublisher feeds a consumer through the routed shm path") {
    const auto channel = std::string("/test/replay/publish/routed");

    auto publisher_bus = std::make_shared<ShmRoutedMessageBus>();
    REQUIRE(publisher_bus->shm_available());
    publisher_bus->configure_route_plan(channel, "");

    auto consumer_bus = std::make_shared<ShmRoutedMessageBus>();
    REQUIRE(consumer_bus->shm_available());
    consumer_bus->configure_route_plan("", channel);

    auto mutex = std::mutex{};
    auto received = std::vector<uint8_t>{};
    auto count = std::atomic<int>{0};
    auto descriptor = ChannelDescriptor{};
    descriptor.name = channel;
    const auto reader = consumer_bus->create_reader(descriptor, [&](const MessageView& message) {
        if (const auto slot = message.payload.as<ShmSlotView>()) {
            std::lock_guard<std::mutex> lock(mutex);
            received.assign(slot->data, slot->data + slot->size);
            count.fetch_add(1, std::memory_order_relaxed);
        }
    });

    auto publisher = MessagePublisher{*publisher_bus};
    const auto payload = std::vector<uint8_t>{0xAA, 0xBB, 0xCC};
    REQUIRE(publisher.publish(make_entry(channel, "test.Msg", star::schema::SchemaTypeId{"test.replay.msg", 1, 0x11}, payload)).ok());

    for (int i = 0; i < 200 && count.load(std::memory_order_relaxed) == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE_EQ(count.load(std::memory_order_relaxed), 1);
    std::lock_guard<std::mutex> lock(mutex);
    REQUIRE_EQ(received, payload);
}

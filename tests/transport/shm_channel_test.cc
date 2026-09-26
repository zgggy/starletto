#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "basement/transport/shm/shm_channel.h"
#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_views.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::transport::ChannelDescriptor;
using star::transport::MessageSerializer;
using star::transport::MessageView;
using star::transport::SerializedMessage;
using star::transport::ShmChannel;
using star::transport::ShmMessageBus;
using star::transport::ShmSlotView;

// shm 名称长度受限（macOS PSHMNAMLEN=32 含结尾 NUL），必须短且定长且各用例唯一
auto make_name() -> std::string {
    static auto next = std::atomic<uint32_t>{1};
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "/st_t_%04x%04x", static_cast<unsigned>(::getpid() & 0xffff), static_cast<unsigned>(next.fetch_add(1) & 0xffff));
    return std::string(buf);
}

auto make_channel() -> std::shared_ptr<ShmChannel> { return std::make_shared<ShmChannel>(make_name(), 1024 * 1024); }

TEST_CASE("ShmChannel write and read roundtrip") {
    const auto channel = make_channel();
    const auto payload = std::vector<uint8_t>(64, 0xAB);
    channel->write(payload.data(), static_cast<uint32_t>(payload.size()));

    uint64_t last_seq = 0;
    const auto result = channel->read(&last_seq);
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->size, payload.size());
    REQUIRE_EQ(result->dropped, 0ULL);
    REQUIRE(result->seq >= 1ULL);

    const auto* data = channel->payload_ptr(result->slot);
    REQUIRE(data != nullptr);
    REQUIRE(std::memcmp(data, payload.data(), payload.size()) == 0);
}

TEST_CASE("ShmChannel read reports nothing new when the sequence is unchanged") {
    const auto channel = make_channel();
    const auto payload = std::vector<uint8_t>(32, 0x01);
    channel->write(payload.data(), static_cast<uint32_t>(payload.size()));

    uint64_t last_seq = 0;
    REQUIRE(channel->read(&last_seq).has_value());
    REQUIRE(not channel->read(&last_seq).has_value());
}

// P0-1 回归测试：槽位按 seq % slot_count 覆写，延迟解码必须被拒绝而非读到撕裂数据
TEST_CASE("ShmSlotView rejects payload overwritten before decode") {
    const auto channel = make_channel();
    const auto first = std::vector<uint8_t>(64, 0x11);
    channel->write(first.data(), static_cast<uint32_t>(first.size()));

    uint64_t last_seq = 0;
    const auto result = channel->read(&last_seq);
    REQUIRE(result.has_value());

    auto view = ShmSlotView{};
    view.data = channel->payload_ptr(result->slot);
    view.size = result->size;
    view.seq_addr = channel->seq_addr();
    view.observed_seq = result->seq;
    view.slot_count = channel->slot_count();

    // 未被覆写时视图稳定且内容完整
    REQUIRE(view.stable());
    REQUIRE_EQ(view.bytes().size(), 64U);
    REQUIRE_EQ(view.bytes()[0], 0x11);
    REQUIRE_EQ(view.text().size(), 64U);

    // 连续写满环形缓冲，强制覆写该视图指向的槽位
    const auto later = std::vector<uint8_t>(64, 0x22);
    for (int i = 0; i < 8; ++i) {
        channel->write(later.data(), static_cast<uint32_t>(later.size()));
    }

    REQUIRE(not view.stable());
    REQUIRE(view.bytes().empty());
    REQUIRE(view.text().empty());
    REQUIRE(not view.as_fixed_layout().valid());
}

// 回归测试：槽位按 seq % slot_count 分配，写入其它槽不得误判本视图失效
// （曾导致 service 调用超时：全局 seq 判等会把同通道并发写的无关视图集体作废）
TEST_CASE("ShmSlotView survives writes to other slots") {
    const auto channel = make_channel();
    const auto first = std::vector<uint8_t>(64, 0x55);
    channel->write(first.data(), static_cast<uint32_t>(first.size()));

    uint64_t last_seq = 0;
    const auto result = channel->read(&last_seq);
    REQUIRE(result.has_value());

    auto view = ShmSlotView{};
    view.data = channel->payload_ptr(result->slot);
    view.size = result->size;
    view.seq_addr = channel->seq_addr();
    view.observed_seq = result->seq;
    view.slot_count = channel->slot_count();
    REQUIRE(view.slot_count > 1U);

    const auto other = std::vector<uint8_t>(64, 0x66);
    for (uint32_t i = 1; i < channel->slot_count(); ++i) {
        channel->write(other.data(), static_cast<uint32_t>(other.size()));
    }

    // 只写满了其它 slot_count-1 个槽，本槽尚未被复用，视图必须仍然可用
    REQUIRE(view.stable());
    REQUIRE_EQ(view.bytes().size(), 64U);
    REQUIRE_EQ(view.bytes()[0], 0x55);

    // 再写一次即复用本槽，此后视图必须失效
    channel->write(other.data(), static_cast<uint32_t>(other.size()));
    REQUIRE(not view.stable());
    REQUIRE(view.bytes().empty());
}

TEST_CASE("ShmSlotView without generation info stays readable") {
    // 未接线代际信息的视图（如手工构造）不参与复核，保持原语义
    const auto buffer = std::vector<uint8_t>(16, 0x7F);
    auto view = ShmSlotView{};
    view.data = buffer.data();
    view.size = buffer.size();

    REQUIRE(view.stable());
    REQUIRE_EQ(view.bytes().size(), 16U);
    REQUIRE_EQ(view.bytes()[0], 0x7F);
}

TEST_CASE("shm delivered view becomes unstable after the slot is overwritten") {
    const auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = ChannelDescriptor{};
    descriptor.name = "test.shm.stale";
    descriptor.type_name = "test.Msg";

    auto mutex = std::mutex{};
    auto captured = std::optional<MessageView>{};
    const auto reader = bus->create_reader(descriptor, [&](const MessageView& message) {
        std::lock_guard<std::mutex> lock(mutex);
        if (not captured.has_value()) {
            captured = message;
        }
    });

    const auto writer = bus->create_writer(descriptor);
    const auto publish = [&](uint8_t fill) {
        auto message = SerializedMessage{};
        message.view.payload = TypedConstPtr::from(std::make_shared<const std::vector<uint8_t>>(64, fill));
        message.serializer = MessageSerializer{};
        message.serializer.payload_size_hint = 64;
        message.serializer.encode_binary = [fill]() { return std::vector<uint8_t>(64, fill); };
        return writer->write_serialized(message);
    };

    // P1-8 回归：基线在订阅时捕获，读者建立后立即发布的首帧必须送达
    REQUIRE(publish(0x33).ok());
    for (int i = 0; i < 200 and not captured.has_value(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(captured.has_value());

    auto held = std::optional<ShmSlotView>{};
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto slot_view = captured->payload.as<ShmSlotView>();
        REQUIRE(slot_view != nullptr);
        held = *slot_view;
    }
    REQUIRE(held->stable());

    // 覆写环形缓冲后，被持有的旧视图必须失效
    for (int i = 0; i < 8; ++i) {
        REQUIRE(publish(static_cast<uint8_t>(0x40 + i)).ok());
    }

    REQUIRE(not held->stable());
    REQUIRE(held->bytes().empty());
}

// P1-8 回归测试：订阅后立即发布的消息不得因轮询线程启动竞态而丢失
TEST_CASE("shm reader receives messages published immediately after subscribe") {
    constexpr int kRounds = 30;
    for (int round = 0; round < kRounds; ++round) {
        const auto bus = std::make_shared<ShmMessageBus>();
        auto descriptor = ChannelDescriptor{};
        descriptor.name = "test.shm.immediate." + std::to_string(round);
        descriptor.type_name = "test.Msg";

        auto received = std::atomic<int>{0};
        const auto reader = bus->create_reader(descriptor, [&](const MessageView&) { received.fetch_add(1); });

        // 紧接着发布，不给轮询线程建立基线留余量
        const auto writer = bus->create_writer(descriptor);
        auto message = SerializedMessage{};
        message.view.payload = TypedConstPtr::from(std::make_shared<const std::vector<uint8_t>>(32, 0x5A));
        message.serializer = MessageSerializer{};
        message.serializer.payload_size_hint = 32;
        message.serializer.encode_binary = []() { return std::vector<uint8_t>(32, 0x5A); };
        REQUIRE(writer->write_serialized(message).ok());

        for (int i = 0; i < 250 and received.load() == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        REQUIRE(received.load() > 0);
    }
}

}  // namespace

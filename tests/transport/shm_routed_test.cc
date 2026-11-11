#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "basement/transport/api/channel_registry.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::transport::ChannelDescriptor;
using star::transport::ChannelRegistry;
using star::transport::MessageSerializer;
using star::transport::MessageView;
using star::transport::Reader;
using star::transport::SerializedMessage;
using star::transport::ShmRoutedMessageBus;
using star::transport::WriteStatus;

struct RoutedMsg {
    int value{0};
};

auto make_descriptor(const std::string& name) -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = name;
    descriptor.type_name = "test.Routed";
    return descriptor;
}

auto make_serialized(int value) -> SerializedMessage {
    auto message = SerializedMessage{};
    message.view.payload = TypedConstPtr::from(std::make_shared<const RoutedMsg>(RoutedMsg{value}));
    message.serializer = MessageSerializer{};
    message.serializer.payload_size_hint = sizeof(RoutedMsg);
    message.serializer.encode_binary = [value]() {
        auto out = std::vector<uint8_t>(sizeof(int));
        std::memcpy(out.data(), &value, sizeof(int));
        return out;
    };
    return message;
}

struct Collector {
    mutable std::mutex mutex;
    std::vector<int> values;

    auto callback() -> Reader::Callback {
        return [this](const MessageView& message) {
            const auto payload = message.payload.as<RoutedMsg>();
            std::lock_guard<std::mutex> lock(mutex);
            values.push_back(payload ? payload->value : -1);
        };
    }

    auto snapshot() const -> std::vector<int> {
        std::lock_guard<std::mutex> lock(mutex);
        return values;
    }
};

// routed 总线的核心不变量：同一条消息只能投递一次。
// inproc 与 shm 两条路径并存时若不做去重，读者会收到两份副本。
TEST_CASE("ShmRoutedMessageBus delivers each message exactly once without a route plan") {
    const auto registry = std::make_shared<ChannelRegistry>();
    const auto bus = std::make_shared<ShmRoutedMessageBus>(registry);
    const auto descriptor = make_descriptor("test.routed.noplan");

    auto collector = Collector{};
    const auto reader = bus->create_reader(descriptor, collector.callback());
    const auto writer = bus->create_writer(descriptor);

    for (int i = 0; i < 20; ++i) {
        REQUIRE(writer->write_serialized(make_serialized(i)).ok());
    }

    for (int i = 0; i < 100; ++i) {
        if (collector.snapshot().size() >= 20) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto values = collector.snapshot();
    REQUIRE_EQ(values.size(), 20U);
    for (int i = 0; i < 20; ++i) {
        REQUIRE_EQ(values[static_cast<size_t>(i)], i);
    }
}

TEST_CASE("ShmRoutedMessageBus delivers each message exactly once with shm routing enabled") {
    const auto registry = std::make_shared<ChannelRegistry>();
    const auto bus = std::make_shared<ShmRoutedMessageBus>(registry);
    const auto descriptor = make_descriptor("test.routed.shm");

    // 把该通道的读写都路由到 shm，此时 inproc 与 shm 两条路径同时存在
    bus->configure_route_plan("test.routed.shm", "test.routed.shm");

    auto collector = Collector{};
    const auto reader = bus->create_reader(descriptor, collector.callback());
    const auto writer = bus->create_writer(descriptor);

    for (int i = 0; i < 20; ++i) {
        REQUIRE(writer->write_serialized(make_serialized(i)).ok());
    }

    for (int i = 0; i < 100; ++i) {
        if (collector.snapshot().size() >= 20) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto values = collector.snapshot();
    // 核心断言：不得出现重复投递（20 条消息只应收到 20 份）
    REQUIRE_EQ(values.size(), 20U);
    auto seen = std::vector<bool>(20, false);
    for (const auto value : values) {
        REQUIRE(value >= 0);
        REQUIRE(value < 20);
        REQUIRE(not seen[static_cast<size_t>(value)]);
        seen[static_cast<size_t>(value)] = true;
    }
}

// P0-4 回归：单帧超限是「这一帧太大」，不是通道故障。
// 旧实现会把它当 EXCEPTION 永久降级整条通道，后续正常帧再也发不出去。
TEST_CASE("an oversized message does not permanently degrade the channel") {
    const auto registry = std::make_shared<ChannelRegistry>();
    const auto bus = std::make_shared<ShmRoutedMessageBus>(registry);
    const auto descriptor = make_descriptor("test.routed.oversize");
    bus->configure_route_plan("test.routed.oversize", "test.routed.oversize");

    auto received = std::atomic<int>{0};
    const auto reader = bus->create_reader(descriptor, [&](const MessageView&) { received.fetch_add(1); });
    const auto writer = bus->create_writer(descriptor);

    // 超过单槽上限（1MiB）一帧
    constexpr size_t kOversize = 1024U * 1024U + 1U;
    auto huge = SerializedMessage{};
    huge.view.payload = TypedConstPtr::from(std::make_shared<const std::vector<uint8_t>>(kOversize, 0xAA));
    huge.serializer = MessageSerializer{};
    huge.serializer.payload_size_hint = kOversize;
    huge.serializer.encode_binary = []() { return std::vector<uint8_t>(kOversize, 0xAA); };
    const auto big = writer->write_serialized(huge);

    REQUIRE(not big.ok());
    REQUIRE(big.status == WriteStatus::ENCODE_FAILED);  // 单帧编码失败，不是通道异常

    // 后续正常帧必须仍然成功且能被收到
    for (int i = 0; i < 10; ++i) {
        REQUIRE(writer->write_serialized(make_serialized(i)).ok());
    }
    for (int i = 0; i < 200 && received.load() < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(received.load() >= 10);
}

TEST_CASE("ShmRoutedMessageBus rejects an empty reader callback") {
    const auto registry = std::make_shared<ChannelRegistry>();
    const auto bus = std::make_shared<ShmRoutedMessageBus>(registry);

    auto threw = false;
    try {
        (void)bus->create_reader(make_descriptor("test.routed.nocb"), nullptr);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

}  // namespace

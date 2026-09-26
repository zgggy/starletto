#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <thread>
#include <vector>

#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_views.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::transport::ChannelDescriptor;
using star::transport::MessageSerializer;
using star::transport::MessageView;
using star::transport::Reader;
using star::transport::SerializedMessage;
using star::transport::ShmMessageBus;
using star::transport::ShmSlotView;

// 载荷带自校验信息：seq 冗余编码进填充字节，撕裂读必然暴露为不一致
struct StressPayload {
    uint64_t seq{0};
    uint8_t fill{0};
    uint8_t pad[55]{};
};

constexpr size_t kPayloadBytes = sizeof(StressPayload);

auto encode_payload(uint64_t seq) -> std::vector<uint8_t> {
    auto payload = StressPayload{};
    payload.seq = seq;
    payload.fill = static_cast<uint8_t>(seq & 0xff);
    std::memset(payload.pad, payload.fill, sizeof(payload.pad));
    auto out = std::vector<uint8_t>(kPayloadBytes);
    std::memcpy(out.data(), &payload, kPayloadBytes);
    return out;
}

auto is_torn(const std::span<const uint8_t>& bytes) -> bool {
    if (bytes.size() != kPayloadBytes) {
        return true;
    }
    auto payload = StressPayload{};
    std::memcpy(&payload, bytes.data(), kPayloadBytes);
    const auto expect = static_cast<uint8_t>(payload.seq & 0xff);
    if (payload.fill != expect) {
        return true;
    }
    for (const auto byte : payload.pad) {
        if (byte != expect) {
            return true;
        }
    }
    return false;
}

struct ReaderStats {
    std::atomic<uint64_t> delivered{0};
    std::atomic<uint64_t> rejected{0};
    std::atomic<uint64_t> torn{0};
    std::atomic<bool> misuse{false};
};

// 并发回归：写方高频覆写 4 槽环形缓冲时，延迟解码的读者要么读到完整一致的载荷，
// 要么被 seqlock 判为失效而拒绝——绝不允许读到撕裂数据
TEST_CASE("shm high frequency writes never tear a delayed read") {
    constexpr int kReaderCount = 3;
    constexpr uint64_t kMessages = 300;
    // 读者延迟用于放大「建视图 → 解码」窗口；写方限速确保读者能采样到多帧
    const std::array<int, kReaderCount> kDelaysUs{0, 50, 150};

    const auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = ChannelDescriptor{};
    descriptor.name = "test.shm.stress";
    descriptor.type_name = "test.Stress";

    auto stats = std::array<ReaderStats, kReaderCount>{};
    auto readers = std::vector<std::shared_ptr<Reader>>{};
    readers.reserve(kReaderCount);

    for (int index = 0; index < kReaderCount; ++index) {
        const auto delay_us = kDelaysUs[static_cast<size_t>(index)];
        auto& slot_stats = stats[static_cast<size_t>(index)];
        // 回调运行在读者轮询线程上，不得使用 REQUIRE（会跨线程抛出）
        readers.push_back(bus->create_reader(descriptor, [delay_us, &slot_stats](const MessageView& message) {
            if (delay_us > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
            }
            const auto slot = message.payload.as<ShmSlotView>();
            if (not slot) {
                slot_stats.misuse.store(true);
                return;
            }
            const auto bytes = slot->bytes();
            if (bytes.empty()) {
                // seqlock 判定槽位已被复用：这是预期的安全拒绝，不是失败
                slot_stats.rejected.fetch_add(1);
                return;
            }
            if (is_torn(bytes)) {
                slot_stats.torn.fetch_add(1);
            }
            slot_stats.delivered.fetch_add(1);
        }));
    }

    const auto writer = bus->create_writer(descriptor);
    for (uint64_t seq = 1; seq <= kMessages; ++seq) {
        const auto payload = encode_payload(seq);
        auto message = SerializedMessage{};
        message.view.payload = TypedConstPtr::from(std::make_shared<const std::vector<uint8_t>>(payload));
        message.serializer = MessageSerializer{};
        message.serializer.payload_size_hint = payload.size();
        message.serializer.encode_binary = [payload]() { return payload; };
        REQUIRE(writer->write_serialized(message).ok());
        // 写方限速：否则 read() 只取最新一条并丢弃中间帧，读者采样不足
        std::this_thread::sleep_for(std::chrono::microseconds(300));
    }

    // 留时间让各读者线程处理完积压
    for (int i = 0; i < 100; ++i) {
        auto pending = false;
        for (const auto& reader : readers) {
            pending = pending or reader->has_data();
        }
        if (not pending) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    auto total_delivered = uint64_t{0};
    auto total_rejected = uint64_t{0};
    auto total_torn = uint64_t{0};
    for (const auto& slot_stats : stats) {
        REQUIRE(not slot_stats.misuse.load());
        total_delivered += slot_stats.delivered.load();
        total_rejected += slot_stats.rejected.load();
        total_torn += slot_stats.torn.load();
    }

    // 核心断言：不允许任何撕裂读
    REQUIRE_EQ(total_torn, 0ULL);
    // 测试必须真的读到过数据，否则等于空转
    REQUIRE(total_delivered > 300);
    std::printf("    [shm-stress] delivered=%llu rejected=%llu torn=%llu\n", static_cast<unsigned long long>(total_delivered), static_cast<unsigned long long>(total_rejected),
                static_cast<unsigned long long>(total_torn));
}

// 反向覆盖：读者必须落后到槽位被复用，验证拒绝路径真的生效
// （否则压力测试只证明了「读得够快时没撕裂」，等于空转）
TEST_CASE("shm lagging reader is rejected rather than served torn data") {
    constexpr uint64_t kMessages = 200;

    const auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = ChannelDescriptor{};
    descriptor.name = "test.shm.lagging";
    descriptor.type_name = "test.Stress";

    auto stats = ReaderStats{};
    // 回调内延迟远大于写入节奏，读者必然落后
    const auto reader = bus->create_reader(descriptor, [&stats](const MessageView& message) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        const auto slot = message.payload.as<ShmSlotView>();
        if (not slot) {
            stats.misuse.store(true);
            return;
        }
        const auto bytes = slot->bytes();
        if (bytes.empty()) {
            stats.rejected.fetch_add(1);
            return;
        }
        if (is_torn(bytes)) {
            stats.torn.fetch_add(1);
        }
        stats.delivered.fetch_add(1);
    });

    // 写入节奏远快于读者处理速度（回调内 3ms），读者必然落后
    const auto writer = bus->create_writer(descriptor);
    for (uint64_t seq = 1; seq <= kMessages; ++seq) {
        const auto payload = encode_payload(seq);
        auto message = SerializedMessage{};
        message.view.payload = TypedConstPtr::from(std::make_shared<const std::vector<uint8_t>>(payload));
        message.serializer = MessageSerializer{};
        message.serializer.payload_size_hint = payload.size();
        message.serializer.encode_binary = [payload]() { return payload; };
        REQUIRE(writer->write_serialized(message).ok());
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    REQUIRE(not stats.misuse.load());
    REQUIRE_EQ(stats.torn.load(), 0ULL);
    // 关键：拒绝必须真的发生，证明 seqlock 保护被触发过
    REQUIRE(stats.rejected.load() >= 2);
    std::printf("    [shm-lag] delivered=%llu rejected=%llu torn=%llu\n", static_cast<unsigned long long>(stats.delivered.load()), static_cast<unsigned long long>(stats.rejected.load()),
                static_cast<unsigned long long>(stats.torn.load()));
}

}  // namespace

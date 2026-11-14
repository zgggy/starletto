#include <atomic>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
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

// P0-1 回归：槽位被复用时必须判定失效（per-slot 代际）
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
    view.gen_addr = channel->gen_addr(result->slot);
    view.observed_gen = result->gen;

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

// P0-2 回归：写方在 loan 与 commit 之间崩溃（_exit 跳过析构），
// 锁与代际都留在中间态。下一个写者必须能在超时后接管，而不是永久阻塞。
TEST_CASE("ShmChannel recovers the write lock from a dead owner") {
    char name[32]{};
    std::snprintf(name, sizeof(name), "/st_recover_%d", static_cast<int>(::getpid()));
    const auto channel = std::make_shared<ShmChannel>(std::string(name), 1024 * 1024);
    const auto payload = std::vector<uint8_t>(64, 0x7E);

    const auto pid = ::fork();
    if (pid == 0) {
        // 子进程：持锁后直接退出，跳过所有析构（模拟崩溃）
        const auto loaned = channel->loan(static_cast<uint32_t>(payload.size()));
        (void)loaned;
        ::_exit(0);
    }
    REQUIRE(pid > 0);

    int status = 0;
    // 必须回收：僵尸进程会让 kill(pid,0) 仍视为存活，从而不触发恢复
    REQUIRE(::waitpid(pid, &status, 0) == pid);

    const auto begin = std::chrono::steady_clock::now();
    channel->write(payload.data(), static_cast<uint32_t>(payload.size()));
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count();

    REQUIRE(elapsed_ms < 5000);                  // 会等一个恢复阈值，但不能无限阻塞
    REQUIRE_EQ(channel->lock_recovery_count(), 1U);

    uint64_t last_seq = 0;
    const auto result = channel->read(&last_seq);
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->size, payload.size());

    // 子进程跳过了析构，ref_count 未减 → 手动清理，避免测试累积泄漏段
    ::shm_unlink(name);
}

// P0-1 核心回归：写方持有 loan（memcpy 进行中、全局 seq 尚未推进）时，
// 本槽必须判为不稳定。旧实现按「全局 seq 推进量 < slot_count」推断，
// 在这个窗口会误判为稳定，从而读到被覆写的撕裂数据。
TEST_CASE("ShmSlotView reports unstable while a writer holds a loan on its slot") {
    const auto channel = make_channel();
    const auto payload = std::vector<uint8_t>(64, 0xAA);
    channel->write(payload.data(), static_cast<uint32_t>(payload.size()));

    uint64_t last_seq = 0;
    const auto result = channel->read(&last_seq);
    REQUIRE(result.has_value());

    auto view = ShmSlotView{};
    view.data = channel->payload_ptr(result->slot);
    view.size = result->size;
    view.gen_addr = channel->gen_addr(result->slot);
    view.observed_gen = result->gen;
    REQUIRE(view.stable());

    // 写满一圈里的其余槽，使下一次 loan 正好落在本视图的槽位
    for (uint32_t i = 1; i < channel->slot_count(); ++i) {
        channel->write(payload.data(), static_cast<uint32_t>(payload.size()));
    }
    REQUIRE(view.stable());  // 本槽尚未被触碰

    {
        // loan 会先把本槽代际置奇（进入写入窗口），但 seq 仍未推进
        const auto loaned = channel->loan(static_cast<uint32_t>(payload.size()));
        REQUIRE_EQ(loaned.slot, result->slot);
        REQUIRE(not view.stable());
        REQUIRE(view.bytes().empty());
    }
    // 未提交即释放：代际恢复为偶，但已是新代际，旧视图必须永久失效
    REQUIRE(not view.stable());
}

// 回归：写入其它槽不得误判本视图失效（同通道并发写不应集体作废）
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
    view.gen_addr = channel->gen_addr(result->slot);
    view.observed_gen = result->gen;
    REQUIRE(channel->slot_count() > 1U);

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

// P0-3 回归：回调在轮询线程上抛异常，不得 terminate 进程；
// 失败的 reader 只影响自己，其他 reader 照常收消息
TEST_CASE("a throwing reader callback on the poll thread does not kill the process") {
    const auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = ChannelDescriptor{};
    descriptor.name = "test.shm.throwing";
    descriptor.type_name = "test.Msg";

    const auto bad = bus->create_reader(descriptor, [](const MessageView&) { throw std::runtime_error("boom"); });
    auto received = std::atomic<int>{0};
    const auto good = bus->create_reader(descriptor, [&](const MessageView&) { received.fetch_add(1); });

    const auto writer = bus->create_writer(descriptor);
    auto message = SerializedMessage{};
    message.view.payload = TypedConstPtr::from(std::make_shared<const std::vector<uint8_t>>(32, 0x5A));
    message.serializer = MessageSerializer{};
    message.serializer.payload_size_hint = 32;
    message.serializer.encode_binary = []() { return std::vector<uint8_t>(32, 0x5A); };
    REQUIRE(writer->write_serialized(message).ok());

    for (int i = 0; i < 200 && received.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    REQUIRE(received.load() > 0);
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

namespace {

auto make_binary_serialized(const std::string& value) -> SerializedMessage {
    auto owner = std::make_shared<const std::vector<uint8_t>>(value.begin(), value.end());
    auto message = SerializedMessage{};
    message.view.payload = TypedConstPtr::from(owner);
    message.serializer.payload_size_hint = owner->size();
    message.serializer.encode_binary = [owner]() { return *owner; };
    return message;
}

}  // namespace

// P2-3：轮询线程属于通道而不是 reader——8 相机 × 3 消费者从 24 线程降到 8 线程
TEST_CASE("readers on the same channel share a single polling thread") {
    const auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = ChannelDescriptor{};
    descriptor.name = "test.poller.same_channel";

    auto received = std::atomic<int>{0};
    auto reader1 = bus->create_reader(descriptor, [&](const MessageView&) { received.fetch_add(1); });
    auto reader2 = bus->create_reader(descriptor, [&](const MessageView&) { received.fetch_add(1); });
    auto reader3 = bus->create_reader(descriptor, [&](const MessageView&) { received.fetch_add(1); });
    REQUIRE_EQ(bus->poller_count(), size_t{1});

    const auto writer = bus->create_writer(descriptor);
    // 每次写完等 3 个 reader 都收到再写下一帧：环形缓冲会合并连续写入，
    // 一次性写 3 帧时读侧只能看到最新一帧（这是通道语义，不是 poller 的问题）
    for (int i = 1; i <= 3; ++i) {
        REQUIRE(writer->write_serialized(make_binary_serialized("payload")).ok());
        for (int wait = 0; wait < 100 && received.load() < 3 * i; ++wait) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    REQUIRE_EQ(received.load(), 9);

    // 最后一个 reader 退出后线程必须回收
    reader1.reset();
    reader2.reset();
    reader3.reset();
    REQUIRE_EQ(bus->poller_count(), size_t{0});
}

TEST_CASE("each channel with readers owns exactly one polling thread") {
    const auto bus = std::make_shared<ShmMessageBus>();
    auto first = ChannelDescriptor{};
    first.name = "test.poller.channel_a";
    auto second = ChannelDescriptor{};
    second.name = "test.poller.channel_b";

    auto reader_a1 = bus->create_reader(first, [](const MessageView&) {});
    auto reader_a2 = bus->create_reader(first, [](const MessageView&) {});
    auto reader_b = bus->create_reader(second, [](const MessageView&) {});
    REQUIRE_EQ(bus->poller_count(), size_t{2});

    reader_a1.reset();
    reader_a2.reset();
    reader_b.reset();
    REQUIRE_EQ(bus->poller_count(), size_t{0});
}

// P0-2 遗留：进程 _exit/SIGKILL 跳过了析构，段不会被 unlink。
// 新进程 attach 时必须识别「所有持有者都已死」并重建，而不是永久卡在孤儿段上。
TEST_CASE("orphan shm segment is reclaimed after the last holder dies without cleanup") {
    const auto name = make_name();
    const auto payload = std::vector<uint8_t>(32, 0x5A);

    const auto pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        auto channel = std::make_shared<ShmChannel>(name, 1024 * 1024);
        channel->write(payload.data(), static_cast<uint32_t>(payload.size()));
        ::_exit(0);  // 跳过析构：段与持有者记录都留在原地
    }
    int status = 0;
    REQUIRE(::waitpid(pid, &status, 0) == pid);
    REQUIRE(WIFEXITED(status));

    auto channel = std::make_shared<ShmChannel>(name, 1024 * 1024);
    REQUIRE_EQ(channel->orphan_recovery_count(), 1U);

    // 重建后的段是干净的：旧数据不应残留
    uint64_t last_seq = 0;
    REQUIRE(not channel->read(&last_seq).has_value());

    channel->write(payload.data(), static_cast<uint32_t>(payload.size()));
    const auto result = channel->read(&last_seq);
    REQUIRE(result.has_value());
    REQUIRE_EQ(result->size, payload.size());

    channel.reset();
    // 最后一个持有者正常退出后段应被 unlink
    errno = 0;
    const auto fd = ::shm_open(name.c_str(), O_RDWR, 0666);
    REQUIRE(fd < 0);
    REQUIRE(errno == ENOENT);
}

// 还有存活持有者时绝不能重建：否则会把正在使用的段清空
TEST_CASE("shm segment with a live holder is never treated as orphan") {
    const auto name = make_name();
    auto first = std::make_shared<ShmChannel>(name, 1024 * 1024);
    const auto payload = std::vector<uint8_t>(16, 0x33);
    first->write(payload.data(), static_cast<uint32_t>(payload.size()));

    int ready_pipe[2]{};
    REQUIRE(::pipe(ready_pipe) == 0);
    const auto pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::close(ready_pipe[0]);
        auto child_channel = std::make_shared<ShmChannel>(name, 1024 * 1024);
        const char ready = 'R';
        (void)::write(ready_pipe[1], &ready, 1);
        // 持有段直到被 SIGKILL
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    ::close(ready_pipe[1]);
    char ready = 0;
    REQUIRE(::read(ready_pipe[0], &ready, 1) == 1);
    ::close(ready_pipe[0]);

    // 同一进程再开一个实例：存活持有者 >0，不能重建
    auto second = std::make_shared<ShmChannel>(name, 1024 * 1024);
    REQUIRE_EQ(second->orphan_recovery_count(), 0U);

    ::kill(pid, SIGKILL);
    int status = 0;
    REQUIRE(::waitpid(pid, &status, 0) == pid);

    // 子进程死后，父进程的实例仍存活：新的 attach 只清扫死槽，不重建
    auto third = std::make_shared<ShmChannel>(name, 1024 * 1024);
    REQUIRE_EQ(third->orphan_recovery_count(), 0U);

    uint64_t last_seq = 0;
    const auto result = third->read(&last_seq);
    REQUIRE(result.has_value());
    const auto* data = third->payload_ptr(result->slot);
    REQUIRE(data != nullptr);
    REQUIRE(std::memcmp(data, payload.data(), payload.size()) == 0);
}

// 安全基线：段必须由属主创建/可读写，其它用户不能注入
TEST_CASE("shm segments are created owner-only") {
    const auto name = make_name();
    auto channel = std::make_shared<ShmChannel>(name, 1024 * 1024);

    const auto fd = ::shm_open(name.c_str(), O_RDWR, 0600);
    REQUIRE(fd >= 0);
    struct stat st{};
    REQUIRE(::fstat(fd, &st) == 0);
    REQUIRE_EQ(st.st_uid, ::geteuid());
    REQUIRE_EQ(st.st_mode & 0777, 0600);
    ::close(fd);
}

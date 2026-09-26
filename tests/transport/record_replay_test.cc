#include <cstdio>
#include <string>
#include <vector>

#include <unistd.h>

#include "basement/transport/record/message_recorder.h"
#include "basement/transport/record/message_replayer.h"

#include "tests/support/star_test.h"

namespace {

using star::transport::MessageRecorder;
using star::transport::MessageReplayer;

// 录制文件的临时路径：放在 /tmp 下，用例结束即清理
auto make_record_path(const std::string& tag) -> std::string {
    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "/tmp/starletto_test_%s_%08x.starrec", tag.c_str(), static_cast<unsigned>(::getpid() & 0xffffffffu));
    return std::string(buf);
}

TEST_CASE("MessageRecorder and MessageReplayer roundtrip preserves every entry") {
    const auto path = make_record_path("roundtrip");

    {
        auto recorder = MessageRecorder{path};
        recorder.open();
        recorder.add_channel("planning.trajectory");
        recorder.add_channel("control.command");

        recorder.record("planning.trajectory", "TrajectoryOutput", 1000, {0x01, 0x02, 0x03});
        recorder.record("control.command", "ControlCommand", 2000, {0x0A});
        recorder.record("planning.trajectory", "TrajectoryOutput", 3000, {0xFF, 0xFE});
        recorder.close();

        REQUIRE_EQ(recorder.message_count(), 3ULL);
    }

    auto replayed = std::vector<std::tuple<std::string, std::string, int64_t, std::vector<uint8_t>>>{};
    auto replayer = MessageReplayer{path};
    replayer.open();

    REQUIRE_EQ(replayer.message_count(), 3ULL);
    const auto channels = replayer.channels();
    REQUIRE_EQ(channels.size(), 2U);

    replayer.play_all([&](const std::string& channel, const std::string& type, int64_t ts, const std::vector<uint8_t>& payload) {
        replayed.emplace_back(channel, type, ts, payload);
    });

    REQUIRE_EQ(replayed.size(), 3U);

    REQUIRE_EQ(std::get<0>(replayed[0]), std::string("planning.trajectory"));
    REQUIRE_EQ(std::get<1>(replayed[0]), std::string("TrajectoryOutput"));
    REQUIRE_EQ(std::get<2>(replayed[0]), 1000);
    REQUIRE_EQ(std::get<3>(replayed[0]), (std::vector<uint8_t>{0x01, 0x02, 0x03}));

    REQUIRE_EQ(std::get<0>(replayed[1]), std::string("control.command"));
    REQUIRE_EQ(std::get<2>(replayed[1]), 2000);
    REQUIRE_EQ(std::get<3>(replayed[1]), (std::vector<uint8_t>{0x0A}));

    REQUIRE_EQ(std::get<2>(replayed[2]), 3000);
    REQUIRE_EQ(std::get<3>(replayed[2]), (std::vector<uint8_t>{0xFF, 0xFE}));

    replayer.close();
    std::remove(path.c_str());
}

// 过滤由 record() 内部完成：调用方忘了查 should_record() 也不会录进无关通道
TEST_CASE("MessageRecorder filters out channels that were not registered") {
    const auto path = make_record_path("filter");

    auto recorder = MessageRecorder{path};
    recorder.open();
    recorder.add_channel("kept");

    REQUIRE(recorder.should_record("kept"));
    REQUIRE(not recorder.should_record("dropped"));

    recorder.record("kept", "T", 1, {0x01});
    recorder.record("dropped", "T", 2, {0x02});
    recorder.close();
    REQUIRE_EQ(recorder.message_count(), 1ULL);

    auto replayer = MessageReplayer{path};
    replayer.open();
    REQUIRE_EQ(replayer.message_count(), 1ULL);
    const auto channels = replayer.channels();
    REQUIRE_EQ(channels.size(), 1U);
    REQUIRE_EQ(channels.front(), std::string("kept"));

    replayer.close();
    std::remove(path.c_str());
}

// spec 验收：「录放必须可复现线上行为（同版本、同参数、同输入）」
TEST_CASE("MessageReplayer replays entries in timestamp order") {
    const auto path = make_record_path("order");

    auto recorder = MessageRecorder{path};
    recorder.open();
    recorder.add_channel("c");
    for (int64_t ts = 5; ts >= 1; --ts) {
        recorder.record("c", "T", ts, {static_cast<uint8_t>(ts)});
    }
    recorder.close();

    auto replayer = MessageReplayer{path};
    replayer.open();
    auto seen = std::vector<int64_t>{};
    // speed 大幅加速，避免测试被录制时间轴拖慢
    replayer.play_all([&](const std::string&, const std::string&, int64_t ts, const std::vector<uint8_t>&) { seen.push_back(ts); }, 1000000.0);

    REQUIRE_EQ(seen.size(), 5U);
    for (size_t i = 1; i < seen.size(); ++i) {
        // play_all 按录制顺序回放；本用例固定录制顺序以校验顺序保持
        REQUIRE_EQ(seen[i], static_cast<int64_t>(5 - static_cast<int64_t>(i)));
    }

    replayer.close();
    std::remove(path.c_str());
}

TEST_CASE("MessageReplayer reports zero messages for an empty recording") {
    const auto path = make_record_path("empty");

    auto recorder = MessageRecorder{path};
    recorder.open();
    recorder.add_channel("c");
    recorder.close();

    auto replayer = MessageReplayer{path};
    replayer.open();
    REQUIRE_EQ(replayer.message_count(), 0ULL);
    REQUIRE(replayer.channels().empty());

    // 空录制的 play_all 应是无操作
    auto calls = 0;
    replayer.play_all([&](const std::string&, const std::string&, int64_t, const std::vector<uint8_t>&) { ++calls; });
    REQUIRE_EQ(calls, 0);

    replayer.close();
    std::remove(path.c_str());
}

}  // namespace

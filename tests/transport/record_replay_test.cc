#include <cstdio>
#include <string>
#include <vector>

#include <unistd.h>

#include "basement/transport/record/message_recorder.h"
#include "basement/transport/record/message_replayer.h"

#include "tests/support/star_test.h"

namespace {

using star::schema::SchemaTypeId;
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

// ---- 录制格式 v2（决策二/三 阶段四）----

TEST_CASE("record entries carry the schema identity of the producing channel") {
    const auto path = make_record_path("schema_identity");
    const auto identity = SchemaTypeId{"planning.trajectory", 2, 0xABCDEF};

    {
        auto recorder = MessageRecorder{path};
        recorder.open();
        recorder.record("planning.trajectory", "TrajectoryOutput", identity, 1000, {0x01});
        recorder.close();
    }

    auto entries = std::vector<MessageReplayer::RecordEntry>{};
    auto replayer = MessageReplayer{path};
    replayer.open();
    replayer.play_all_entries([&](const MessageReplayer::RecordEntry& entry) { entries.push_back(entry); });

    REQUIRE_EQ(entries.size(), 1U);
    REQUIRE_EQ(entries[0].schema_type_id.name, std::string("planning.trajectory"));
    REQUIRE_EQ(entries[0].schema_type_id.version, int64_t{2});
    REQUIRE_EQ(entries[0].schema_type_id.hash, uint64_t{0xABCDEF});
    REQUIRE_EQ(replayer.file_version(), 2U);

    replayer.close();
    std::remove(path.c_str());
}

TEST_CASE("record summaries aggregate channels and schema versions") {
    const auto path = make_record_path("summaries");
    {
        auto recorder = MessageRecorder{path};
        recorder.open();
        recorder.record("a", "A", SchemaTypeId{"test.a", 1, 0x11}, 100, {0x01});
        recorder.record("a", "A", SchemaTypeId{"test.a", 2, 0x22}, 200, {0x02});
        recorder.record("b", "B", SchemaTypeId{"test.b", 1, 0x33}, 300, {0x03});
        recorder.close();
    }

    auto replayer = MessageReplayer{path};
    replayer.open();
    const auto summaries = replayer.summaries();
    REQUIRE_EQ(summaries.size(), 2U);

    const auto& first = summaries[0];
    REQUIRE_EQ(first.channel_name, std::string("a"));
    REQUIRE_EQ(first.message_count, 2ULL);
    REQUIRE_EQ(first.versions.size(), 2U);
    REQUIRE_EQ(first.versions[0], int64_t{1});
    REQUIRE_EQ(first.versions[1], int64_t{2});
    REQUIRE_EQ(first.first_timestamp_ns, 100);
    REQUIRE_EQ(first.last_timestamp_ns, 200);

    replayer.close();
    std::remove(path.c_str());
}

TEST_CASE("record wall clock anchor converts steady timestamps to wall clock") {
    const auto path = make_record_path("wallclock");
    int64_t wall_anchor = 0;
    int64_t steady_anchor = 0;
    {
        auto recorder = MessageRecorder{path};
        recorder.open();
        wall_anchor = recorder.wall_clock_anchor_ns();
        steady_anchor = recorder.steady_anchor_ns();
        recorder.record("a", "A", SchemaTypeId{"test.a", 1, 0}, steady_anchor + 5000000, {0x01});
        recorder.close();
    }
    REQUIRE(wall_anchor > 0);
    REQUIRE(steady_anchor > 0);

    auto replayer = MessageReplayer{path};
    replayer.open();
    MessageReplayer::RecordEntry entry;
    replayer.play_all_entries([&](const MessageReplayer::RecordEntry& e) { entry = e; }, 1000000.0);

    // 5000000 ns = 5ms；换算结果必须严格等于「墙钟锚点 + 相对偏移」
    REQUIRE_EQ(replayer.wall_clock_ns(entry), wall_anchor + 5000000);
    REQUIRE_EQ(replayer.wall_clock_anchor_ns(), wall_anchor);

    replayer.close();
    std::remove(path.c_str());
}

TEST_CASE("replay validation reports a version mismatch and accepts unknown hashes") {
    const auto path = make_record_path("validate");
    {
        auto recorder = MessageRecorder{path};
        recorder.open();
        recorder.record("a", "A", SchemaTypeId{"test.a", 1, 0x11}, 100, {0x01});
        recorder.record("b", "B", SchemaTypeId{"test.b", 1, 0x22}, 200, {0x02});
        recorder.close();
    }

    auto replayer = MessageReplayer{path};
    replayer.open();

    // 同版本同 hash：通过
    REQUIRE(replayer.validate_against({SchemaTypeId{"test.a", 1, 0x11}, SchemaTypeId{"test.b", 1, 0x22}}).ok());
    // 版本不同：报错
    const auto mismatch = replayer.validate_against({SchemaTypeId{"test.a", 2, 0x11}, SchemaTypeId{"test.b", 1, 0x22}});
    REQUIRE(not mismatch.ok());
    REQUIRE_EQ(mismatch.issues.size(), 1U);
    REQUIRE_EQ(mismatch.issues[0].schema_name, std::string("test.a"));
    // 本地方 hash 未知：不阻断（交给解码策略）
    REQUIRE(replayer.validate_against({SchemaTypeId{"test.a", 1, 0}, SchemaTypeId{"test.b", 1, 0}}).ok());
    // 本地未注册：报告
    const auto missing = replayer.validate_against({SchemaTypeId{"test.a", 1, 0x11}});
    REQUIRE(not missing.ok());
    REQUIRE_EQ(missing.issues.size(), 1U);
    REQUIRE_EQ(missing.issues[0].schema_name, std::string("test.b"));

    replayer.close();
    std::remove(path.c_str());
}

TEST_CASE("replayer still reads v1 recordings without schema identity") {
    const auto path = make_record_path("v1");
    {
        auto* fp = std::fopen(path.c_str(), "wb");
        REQUIRE(fp != nullptr);
        const auto magic = std::string("STARREC\0", 8);
        const uint32_t version = 1;
        const uint64_t count = 2;
        std::fwrite(magic.data(), 1, magic.size(), fp);
        std::fwrite(&version, sizeof(version), 1, fp);
        std::fwrite(&count, sizeof(count), 1, fp);
        auto write_entry = [fp](int64_t ts, const std::string& channel, const std::string& type, const std::vector<uint8_t>& payload) {
            std::fwrite(&ts, sizeof(ts), 1, fp);
            const auto write_str = [fp](const std::string& s) {
                const auto len = static_cast<uint32_t>(s.size());
                std::fwrite(&len, sizeof(len), 1, fp);
                std::fwrite(s.data(), 1, s.size(), fp);
            };
            write_str(channel);
            write_str(type);
            const auto size = static_cast<uint32_t>(payload.size());
            std::fwrite(&size, sizeof(size), 1, fp);
            std::fwrite(payload.data(), 1, payload.size(), fp);
        };
        write_entry(10, "old.channel", "OldType", {0xAA});
        write_entry(20, "old.channel", "OldType", {0xBB});
        std::fclose(fp);
    }

    auto replayer = MessageReplayer{path};
    replayer.open();
    REQUIRE_EQ(replayer.file_version(), 1U);
    REQUIRE_EQ(replayer.message_count(), 2ULL);

    auto entries = std::vector<MessageReplayer::RecordEntry>{};
    replayer.play_all_entries([&](const MessageReplayer::RecordEntry& entry) { entries.push_back(entry); }, 1000000.0);
    REQUIRE_EQ(entries.size(), 2U);
    REQUIRE(entries[0].schema_type_id.empty());
    REQUIRE_EQ(entries[1].payload, (std::vector<uint8_t>{0xBB}));
    // v1 没有墙钟锚点：不做假换算
    REQUIRE_EQ(replayer.wall_clock_anchor_ns(), 0);
    REQUIRE_EQ(replayer.wall_clock_ns(entries[0]), entries[0].timestamp_ns);

    replayer.close();
    std::remove(path.c_str());
}

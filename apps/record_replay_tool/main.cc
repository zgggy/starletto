#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>

#include "basement/transport/record/message_replayer.h"

static void print_usage() {
    std::cerr << "Usage:\n"
              << "  star record info <record_file>     Show recording file info\n"
              << "  star replay <record_file> [speed]    Replay recording to stdout\n";
}

static auto format_wall_clock(int64_t wall_clock_ns) -> std::string {
    if (wall_clock_ns <= 0) {
        return "unknown (v1 recording)";
    }
    const auto seconds = static_cast<std::time_t>(wall_clock_ns / 1000000000LL);
    std::tm tm_buf{};
    if (::gmtime_r(&seconds, &tm_buf) == nullptr) {
        return std::to_string(wall_clock_ns);
    }
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

static auto cmd_info(const std::string& file_path) -> int {
    try {
        star::transport::MessageReplayer replayer(file_path);
        replayer.open();
        std::cout << "Recording: " << file_path << "\n"
                  << "  format: v" << replayer.file_version() << "\n"
                  << "  messages: " << replayer.message_count() << "\n"
                  << "  wall clock: " << format_wall_clock(replayer.wall_clock_anchor_ns()) << "\n"
                  << "  channels: " << replayer.channels().size() << "\n";

        for (const auto& summary : replayer.summaries()) {
            std::cout << "    " << summary.channel_name << "  type=" << summary.type_name
                      << "  messages=" << summary.message_count;
            if (summary.versions.empty()) {
                std::cout << "  schema=<none>";
            } else {
                std::cout << "  schema=";
                for (size_t i = 0; i < summary.versions.size(); ++i) {
                    if (i > 0) {
                        std::cout << ",";
                    }
                    std::cout << "v" << summary.versions[i] << "/h" << summary.hashes[i];
                }
            }
            std::cout << "  ts=[" << summary.first_timestamp_ns << ".." << summary.last_timestamp_ns << "]\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[E] info failed: " << e.what() << "\n";
        return 1;
    }
}

static auto cmd_replay(const std::string& file_path, double speed) -> int {
    try {
        star::transport::MessageReplayer replayer(file_path);
        replayer.open();
        std::cout << "Replaying: " << file_path << ", messages=" << replayer.message_count() << ", speed=" << speed << "\n";

        int count{0};
        replayer.play_all_entries(
            [&](const star::transport::MessageReplayer::RecordEntry& entry) {
                ++count;
                std::cout << "  [" << count << "] topic=" << entry.channel_name
                          << ", type=" << entry.type_name;
                if (not entry.schema_type_id.empty()) {
                    std::cout << ", schema=" << entry.schema_type_id.name << " v" << entry.schema_type_id.version
                              << " h" << entry.schema_type_id.hash;
                }
                std::cout << ", payload_size=" << entry.payload.size() << "\n";
            },
            speed);

        std::cout << "Replay done: " << count << " messages\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[E] replay failed: " << e.what() << "\n";
        return 1;
    }
}

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    const auto cmd = std::string(argv[1]);
    if (cmd == "info") {
        return cmd_info(argv[2]);
    }
    if (cmd == "replay") {
        double speed = 1.0;
        if (argc >= 4) {
            speed = std::stod(argv[3]);
        }
        return cmd_replay(argv[2], speed);
    }
    print_usage();
    return 1;
}

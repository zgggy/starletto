#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "basement/transport/record/message_replayer.h"

static void print_usage() {
    std::cerr << "Usage:\n"
              << "  star record info <record_file>     Show recording file info\n"
              << "  star replay <record_file> [speed]    Replay recording to stdout\n";
}

static auto cmd_info(const std::string& file_path) -> int {
    try {
        star::transport::MessageReplayer replayer(file_path);
        replayer.open();
        std::cout << "Recording: " << file_path << "\n"
                  << "  messages: " << replayer.message_count() << "\n"
                  << "  channels: ";
        const auto channels = replayer.channels();
        for (size_t i = 0; i < channels.size(); ++i) {
            if (i > 0) {
                std::cout << ", ";
            }
            std::cout << channels[i];
        }
        std::cout << "\n";
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
        replayer.play_all(
            [&](const std::string& topic, const std::string& type_name, int64_t, const std::vector<uint8_t>& payload) {
                ++count;
                std::cout << "  [" << count << "] topic=" << topic << ", type=" << type_name << ", payload_size=" << (int)payload.size() << "\n";
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

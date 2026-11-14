#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <dlfcn.h>

#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/factory/runtime_factory.h"
#include "basement/schema/native/schema_bootstrap.h"
#include "basement/transport/record/message_publisher.h"
#include "basement/transport/record/message_replayer.h"
#include "basement/transport/shm/shm_routed_message_bus.h"

namespace {

struct ReplayOptions {
    double speed{1.0};
    bool publish{false};
    bool strict{false};
    int64_t repeat{1};
    std::string channel;
    std::string transport{"shm_routed"};
    std::vector<std::string> plugins;
};

void print_usage() {
    std::cerr << "Usage:\n"
              << "  star record info <record_file>\n"
              << "  star replay <record_file> [speed] [options]\n"
              << "\n"
              << "Options:\n"
              << "  --publish            把消息发布回通道（默认只打印）\n"
              << "  --channel=<name>     只回放指定通道\n"
              << "  --repeat=<n>         重复次数（默认 1）\n"
              << "  --strict             录制中的 schema 未注册/hash 不一致时失败\n"
              << "  --transport=<name>   发布用传输（默认 shm_routed）\n"
              << "  --plugin=<lib.so>    加载插件以注册其 schema（可重复；用于回放校验）\n";
}

auto format_wall_clock(int64_t wall_clock_ns) -> std::string {
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

// 回放工具不直接依赖业务模块：需要它们的 schema 时按插件加载（插件入口负责注册）
auto load_plugin_schemas(star::runtime::ComponentRegistry& registry, const std::vector<std::string>& plugins) -> bool {
    for (const auto& plugin : plugins) {
        void* handle = ::dlopen(plugin.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const auto* error = ::dlerror();
            std::cerr << "[E] dlopen failed: " << plugin << ", err=" << (error ? error : "unknown") << "\n";
            return false;
        }
        ::dlerror();
        const auto symbol = ::dlsym(handle, "star_register_components");
        const auto* error = ::dlerror();
        if (error != nullptr || symbol == nullptr) {
            std::cerr << "[E] plugin missing star_register_components: " << plugin << "\n";
            ::dlclose(handle);
            return false;
        }
        reinterpret_cast<void (*)(star::runtime::ComponentRegistry*)>(symbol)(&registry);
    }
    return true;
}

auto cmd_info(const std::string& file_path) -> int {
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

auto cmd_replay(const std::string& file_path, const ReplayOptions& options) -> int {
    try {
        star::transport::MessageReplayer replayer(file_path);
        replayer.open();

        auto entries = replayer.entries();
        if (not options.channel.empty()) {
            auto filtered = std::vector<star::transport::MessageReplayer::RecordEntry>{};
            for (auto& entry : entries) {
                if (entry.channel_name == options.channel) {
                    filtered.push_back(entry);
                }
            }
            entries = std::move(filtered);
        }
        if (entries.empty()) {
            std::cout << "nothing to replay (messages=0 or channel filter matched nothing)\n";
            return 0;
        }

        if (not options.publish) {
            std::cout << "Replaying: " << file_path << ", messages=" << replayer.message_count()
                      << ", speed=" << options.speed << " (stdout only; use --publish to feed channels)\n";
            int count{0};
            for (const auto& entry : entries) {
                ++count;
                std::cout << "  [" << count << "] topic=" << entry.channel_name << ", type=" << entry.type_name;
                if (not entry.schema_type_id.empty()) {
                    std::cout << ", schema=" << entry.schema_type_id.name << " v" << entry.schema_type_id.version
                              << " h" << entry.schema_type_id.hash;
                }
                std::cout << ", payload_size=" << entry.payload.size() << "\n";
            }
            std::cout << "Replay done: " << count << " messages\n";
            return 0;
        }

        // 发布模式：先把录制里的 schema 身份和本地注册表比对，未知/不一致要可见
        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport);
        star::schema::native::register_builtin_schemas(context->schema_registry());
        auto plugin_registry = star::runtime::ComponentRegistry{};
        plugin_registry.set_schema_registry(&context->schema_registry());
        if (not load_plugin_schemas(plugin_registry, options.plugins)) {
            return 2;
        }

        auto unknown = std::unordered_set<std::string>{};
        auto mismatched = std::unordered_set<std::string>{};
        auto checked = std::unordered_set<std::string>{};
        for (const auto& entry : entries) {
            const auto& id = entry.schema_type_id;
            if (id.name.empty()) {
                continue;
            }
            const auto key = id.name + "#" + std::to_string(id.version) + "#" + std::to_string(id.hash);
            if (not checked.insert(key).second) {
                continue;
            }
            const auto result = context->schema_registry().lookup(id);
            if (result.status == star::schema::SchemaMatchStatus::kNotFound) {
                unknown.insert(id.name + " v" + std::to_string(id.version));
            } else if (result.status == star::schema::SchemaMatchStatus::kHashMismatch) {
                mismatched.insert(id.name + " v" + std::to_string(id.version) + " h" + std::to_string(id.hash));
            }
        }
        for (const auto& name : unknown) {
            std::cerr << "[W] recorded schema is not registered locally: " << name << "\n";
        }
        for (const auto& name : mismatched) {
            std::cerr << "[W] recorded schema content differs from local: " << name << "\n";
        }
        if (options.strict and (not unknown.empty() or not mismatched.empty())) {
            std::cerr << "[E] strict mode: refusing to publish with unregistered/mismatched schemas\n";
            return 2;
        }

        // 路由：把录制里的通道都声明为本进程发布，routed 总线才会写 shm
        auto channels = std::unordered_set<std::string>{};
        for (const auto& entry : entries) {
            channels.insert(entry.channel_name);
        }
        if (auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context->message_bus())) {
            auto csv = std::string{};
            for (const auto& channel : channels) {
                if (not csv.empty()) {
                    csv.push_back(',');
                }
                csv += channel;
            }
            routed->configure_route_plan(csv, "");
        }

        auto publisher = star::transport::MessagePublisher{context->message_bus()};
        auto attempted = uint64_t{0};
        auto published = uint64_t{0};
        auto failed = uint64_t{0};
        std::string first_error;

        const auto repeats = options.repeat > 0 ? options.repeat : 1;
        for (int64_t round = 0; round < repeats; ++round) {
            auto write_fn = [&](const star::transport::MessageReplayer::RecordEntry& entry) {
                ++attempted;
                const auto result = publisher.publish(entry);
                if (result.ok()) {
                    ++published;
                } else {
                    ++failed;
                    if (first_error.empty()) {
                        first_error = entry.channel_name + ": " + star::transport::write_status_string(result.status) +
                                      (result.error_message.empty() ? "" : (", " + result.error_message));
                    }
                }
            };
            if (options.channel.empty()) {
                replayer.play_all_entries(write_fn, options.speed);
            } else {
                replayer.play_channel_entries(options.channel, write_fn, options.speed);
            }
        }

        std::cout << "Published: " << published << "/" << attempted << " messages to " << channels.size()
                  << " channels, rounds=" << repeats;
        if (failed > 0) {
            std::cout << ", failed=" << failed;
        }
        std::cout << "\n";
        if (not first_error.empty()) {
            std::cerr << "[E] first publish failure: " << first_error << "\n";
        }
        return failed > 0 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "[E] replay failed: " << e.what() << "\n";
        return 1;
    }
}

}  // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    const auto cmd = std::string(argv[1]);
    const auto file_path = std::string(argv[2]);
    if (cmd == "info") {
        return cmd_info(file_path);
    }
    if (cmd != "replay") {
        print_usage();
        return 1;
    }

    auto options = ReplayOptions{};
    for (int i = 3; i < argc; ++i) {
        const auto arg = std::string(argv[i]);
        if (arg == "--publish") {
            options.publish = true;
        } else if (arg == "--strict") {
            options.strict = true;
        } else if (arg.rfind("--channel=", 0) == 0) {
            options.channel = arg.substr(std::string("--channel=").size());
        } else if (arg.rfind("--repeat=", 0) == 0) {
            options.repeat = std::stoll(arg.substr(std::string("--repeat=").size()));
        } else if (arg.rfind("--transport=", 0) == 0) {
            options.transport = arg.substr(std::string("--transport=").size());
        } else if (arg.rfind("--plugin=", 0) == 0) {
            options.plugins.push_back(arg.substr(std::string("--plugin=").size()));
        } else if (arg == "--help" or arg == "-h") {
            print_usage();
            return 0;
        } else {
            options.speed = std::stod(arg);
        }
    }
    return cmd_replay(file_path, options);
}

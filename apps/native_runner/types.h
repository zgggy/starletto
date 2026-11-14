#ifndef __NATIVE_RUNNER_TYPES_H__
#define __NATIVE_RUNNER_TYPES_H__

#include <string>
#include <vector>

namespace star::native_runner {

extern std::string g_argv0;

struct RunOptions {
    int64_t frames{1};
    int64_t sleep_ms{0};
    std::string mode;
    std::string graph_format;
    std::string transport_name;
    std::string process_group;
    std::string route_plan;
    std::string shm_pub_topics;
    std::string shm_sub_topics;
    // 服务等「任意进程都可能读写」的通道：始终走 shm，不依赖路由计划推断生产者
    std::string system_channels;
    bool deep{false};
    std::string log_file;
    std::string log_dir;
    bool multi_process{false};
    bool worker{false};
    int64_t monitor_ms{3000};
    bool monitor_once{false};
};

struct Inputs {
    RunOptions options;
    std::vector<std::string> paths;
};

}  // namespace star::native_runner

#endif  // __NATIVE_RUNNER_TYPES_H__

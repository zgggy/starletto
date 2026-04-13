#ifndef __NATIVE_RUNNER_MAIN_H__
#define __NATIVE_RUNNER_MAIN_H__

#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/factory/runtime_factory.h"
#include "basement/runtime/process/process_orchestrator.h"
#include "apps/native_runner/types.h"
#include "apps/native_runner/app.h"
#include "apps/native_runner/runner_utils.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <dlfcn.h>
#include <execinfo.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <signal.h>
#include <sys/wait.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace star::native_runner {

std::string g_argv0;

static void crash_handler(int sig) {
    void* frames[64]{};
    const auto n = ::backtrace(frames, 64);
    ::fprintf(stderr, "[E] native_runner crashed, signal=%d\n", sig);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
    ::fflush(stderr);
    _exit(128 + sig);
}

static auto parse_i64_arg(const std::string& s, const std::string& prefix) -> std::optional<int64_t> {
    if (s.rfind(prefix, 0) != 0) { return std::nullopt; }
    const auto v = s.substr(prefix.size());
    if (v.empty()) { return std::nullopt; }
    try { return static_cast<int64_t>(std::stoll(v)); }
    catch (...) { return std::nullopt; }
}

static auto parse_args(int argc, char** argv) -> Inputs {
    Inputs inputs;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == nullptr) { continue; }
        auto a = std::string(argv[i]);
        if (a.rfind("--transport=", 0) == 0) {
            const auto v = a.substr(std::string("--transport=").size());
            if (not v.empty()) { inputs.options.transport_name = v; }
            continue;
        }
        if (a.rfind("--shm-pub-topics=", 0) == 0) {
            inputs.options.shm_pub_topics = a.substr(std::string("--shm-pub-topics=").size());
            continue;
        }
        if (a.rfind("--shm-sub-topics=", 0) == 0) {
            inputs.options.shm_sub_topics = a.substr(std::string("--shm-sub-topics=").size());
            continue;
        }
        if (a.rfind("--process-group=", 0) == 0) {
            inputs.options.process_group = a.substr(std::string("--process-group=").size());
            continue;
        }
        if (a.rfind("--route-plan=", 0) == 0) {
            inputs.options.route_plan = a.substr(std::string("--route-plan=").size());
            continue;
        }
        if (a == "--worker") { inputs.options.worker = true; continue; }
        if (a == "--multi-process") { inputs.options.multi_process = true; continue; }
        if (auto v = parse_i64_arg(a, "--frames=")) { inputs.options.frames = *v; continue; }
        if (auto v = parse_i64_arg(a, "--sleep-ms=")) { inputs.options.sleep_ms = *v; continue; }
        if (a == "--list") { inputs.options.mode = "list"; continue; }
        if (a == "--doctor") { inputs.options.mode = "doctor"; continue; }
        if (a == "--monitor") { inputs.options.mode = "monitor"; continue; }
        if (a.rfind("--graph=", 0) == 0) {
            inputs.options.mode = "graph";
            inputs.options.graph_format = a.substr(std::string("--graph=").size());
            continue;
        }
        if (a == "--deep") { inputs.options.deep = true; continue; }
        if (a.rfind("--log-file=", 0) == 0) {
            inputs.options.log_file = a.substr(std::string("--log-file=").size());
            continue;
        }
        if (a.rfind("--log-dir=", 0) == 0) {
            inputs.options.log_dir = a.substr(std::string("--log-dir=").size());
            continue;
        }
        inputs.paths.emplace_back(std::move(a));
    }
    return inputs;
}

}  // namespace star::native_runner

auto main(int argc, char** argv) -> int {
    try {
        ::signal(SIGSEGV, star::native_runner::crash_handler);
        ::signal(SIGBUS, star::native_runner::crash_handler);
        ::signal(SIGABRT, star::native_runner::crash_handler);
        auto inputs = star::native_runner::parse_args(argc, argv);
        if (argv != nullptr && argv[0] != nullptr) {
            star::native_runner::g_argv0 = std::string(argv[0]);
            if (star::native_runner::g_argv0.find('/') == std::string::npos) {
                star::native_runner::g_argv0 = (std::filesystem::current_path() / star::native_runner::g_argv0).string();
            }
        }

        if (inputs.options.log_file.empty() && not inputs.options.log_dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path(inputs.options.log_dir), ec);
            inputs.options.log_file = inputs.options.log_dir + "/starletto_" + star::native_runner::App::make_timestamp() + ".log";
        }
        if (not inputs.options.log_file.empty()) {
            ::setenv("STAR_LOG_FILE", inputs.options.log_file.c_str(), 1);
        }
        star::native_runner::App app;
        if (inputs.options.mode == "list") {
            auto modules = star::runtime::process::normalize_inputs_to_module_conf_paths(inputs.paths);
            return app.list(modules);
        }
        if (inputs.options.mode == "graph") {
            auto modules = star::runtime::process::normalize_inputs_to_module_conf_paths(inputs.paths);
            if (inputs.options.graph_format.empty() || inputs.options.graph_format == "dot" ||
                inputs.options.graph_format == "module-dot") {
                return app.graph_modules_dot(modules);
            }
            if (inputs.options.graph_format == "component-dot") {
                return app.graph_components_dot(modules);
            }
            logE << "unsupported graph format: " << inputs.options.graph_format;
            return 2;
        }
        if (inputs.options.mode == "doctor") {
            auto modules = star::runtime::process::normalize_inputs_to_module_conf_paths(inputs.paths);
            return app.doctor(modules, inputs.options.deep);
        }
        if (inputs.options.mode == "monitor") {
            auto component_confs = star::native_runner::normalize_inputs_to_component_conf_paths(inputs.paths);
            return app.monitor(inputs.options, component_confs);
        }
        auto component_conf_paths = star::native_runner::normalize_inputs_to_component_conf_paths(inputs.paths);
        if (inputs.options.multi_process && not inputs.options.worker) {
            return app.run_multi_process(inputs.options, component_conf_paths);
        }
        return app.run(inputs.options, component_conf_paths);
    } catch (const std::exception& e) {
        logE << "native runner fatal: " << e.what();
        return 1;
    } catch (...) {
        logE << "native runner fatal: unknown exception";
        return 1;
    }
}

#endif  // __NATIVE_RUNNER_MAIN_H__

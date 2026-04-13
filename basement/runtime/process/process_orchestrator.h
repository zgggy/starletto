#ifndef __STAR_RUNTIME_PROCESS_PROCESS_ORCHESTRATOR_H__
#define __STAR_RUNTIME_PROCESS_ORCHESTRATOR_H__

#include <sys/wait.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "basement/core/logger/logger.h"
#include "basement/runtime/plan/plan_builder.h"

namespace star::runtime::process {

struct WorkerLaunchConfig {
    std::string group;
    std::vector<std::string> component_conf_paths;
    std::string route_plan;
    int64_t frames{0};
    int sleep_ms{0};
    std::string log_file;
};

struct ProcessResult {
    bool ok{true};
    std::vector<std::pair<std::string, int>> exited_workers;
};

class ProcessOrchestrator {
  public:
    explicit ProcessOrchestrator(const std::string& argv0) : argv0_(argv0) {}

    auto build_route_plan(const std::unordered_map<std::string, std::string>& channel_producers,
                          const std::unordered_map<std::string, std::unordered_set<std::string>>& channel_consumers) const -> std::string {
        auto parts = std::vector<std::string>{};
        for (const auto& [channel, consumers] : channel_consumers) {
            const auto it = channel_producers.find(channel);
            if (it == channel_producers.end()) {
                continue;
            }
            if (consumers.count(it->second) > 0) {
                continue;
            }
            auto entry = channel + "|" + it->second + "|";
            bool first = true;
            for (const auto& c : consumers) {
                if (not first) {
                    entry += ",";
                }
                entry += c;
                first = false;
            }
            parts.emplace_back(std::move(entry));
        }
        std::sort(parts.begin(), parts.end());
        auto out = std::string{};
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                out += ";";
            }
            out += parts[i];
        }
        return out;
    }

    auto launch_worker(const WorkerLaunchConfig& config) const -> pid_t {
        auto argv = std::vector<std::string>{};
        argv.emplace_back(argv0_.empty() ? "native_runner" : argv0_);
        argv.emplace_back("--worker");
        argv.emplace_back("--transport=shm_routed");
        argv.emplace_back("--process-group=" + config.group);
        if (not config.route_plan.empty()) {
            argv.emplace_back("--route-plan=" + config.route_plan);
        }
        argv.emplace_back("--frames=" + std::to_string(config.frames));
        if (config.sleep_ms > 0) {
            argv.emplace_back("--sleep-ms=" + std::to_string(config.sleep_ms));
        }
        if (not config.log_file.empty()) {
            argv.emplace_back("--log-file=" + config.log_file);
        }
        for (const auto& p : config.component_conf_paths) {
            argv.emplace_back(p);
        }

        auto cargv = std::vector<char*>{};
        cargv.reserve(argv.size() + 1);
        for (auto& s : argv) {
            cargv.push_back(s.data());
        }
        cargv.push_back(nullptr);

        const auto pid = fork();
        if (pid == 0) {
            execv(cargv[0], cargv.data());
            _exit(127);
        }
        if (pid < 0) {
            logE << "ProcessOrchestrator fork failed for group: " << config.group;
            return -1;
        }
        return pid;
    }

    auto wait_all(const std::vector<pid_t>& pids, const std::unordered_map<pid_t, std::string>& pid_group) const -> ProcessResult {
        auto result = ProcessResult{};
        for (const auto pid : pids) {
            int status = 0;
            const auto w = waitpid(pid, &status, 0);
            if (w < 0) {
                result.ok = false;
                continue;
            }
            const auto& group = pid_group.at(pid);
            if (WIFEXITED(status)) {
                const auto code = WEXITSTATUS(status);
                if (code != 0) {
                    result.ok = false;
                    logE << "[Supervisor] worker exited non-zero: group=" << group << ", code=" << code;
                } else {
                    logI << "[Supervisor] worker exited ok: group=" << group;
                }
                result.exited_workers.emplace_back(group, code);
                continue;
            }
            if (WIFSIGNALED(status)) {
                result.ok = false;
                logE << "[Supervisor] worker killed by signal: group=" << group << ", sig=" << WTERMSIG(status);
                result.exited_workers.emplace_back(group, -WTERMSIG(status));
            } else {
                result.ok = false;
                logE << "[Supervisor] worker exited abnormally: group=" << group;
                result.exited_workers.emplace_back(group, -1);
            }
        }
        return result;
    }

    static auto supervisor_summary(const ProcessResult& result) -> std::string {
        auto oss = std::ostringstream{};
        oss << "=== Supervisor Summary ===\n";
        oss << "  overall: " << (result.ok ? "OK" : "FAILED") << "\n";
        oss << "  workers: " << result.exited_workers.size() << "\n";
        for (const auto& [group, code] : result.exited_workers) {
            if (code == 0) {
                oss << "    " << group << ": OK\n";
            } else if (code < 0) {
                oss << "    " << group << ": SIGNAL(" << (-code) << ")\n";
            } else {
                oss << "    " << group << ": EXIT(" << code << ")\n";
            }
        }
        return oss.str();
    }

  private:
    std::string argv0_;
};

}  // namespace star::runtime::process

#endif  // __STAR_RUNTIME_PROCESS_PROCESS_ORCHESTRATOR_H__

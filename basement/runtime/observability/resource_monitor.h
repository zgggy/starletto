#ifndef __RESOURCE_MONITOR_H__
#define __RESOURCE_MONITOR_H__

#include <chrono>
#include <cstdint>
#include <dirent.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace star::runtime::observability {

struct ResourceSnapshot {
    int64_t pid{0};
    std::string name;
    double cpu_percent{0.0};
    uint64_t rss_kb{0};
    int fd_count{0};
    int64_t thread_count{0};
    int64_t timestamp_ns{0};

    auto format_line() const -> std::string {
        char buf[256]{};
        std::snprintf(buf, sizeof(buf),
                     "resource pid=%ld name=%s cpu=%.1f%% rss=%lukB fds=%d threads=%ld",
                     static_cast<long>(pid), name.c_str(), cpu_percent,
                     static_cast<unsigned long>(rss_kb), fd_count,
                     static_cast<long>(thread_count));
        return std::string(buf);
    }
};

class ResourceMonitor {
  public:
    auto sample_self() -> ResourceSnapshot {
        return sample(getpid());
    }

    auto sample(int64_t pid) -> ResourceSnapshot {
        auto snap = ResourceSnapshot{};
        snap.pid = pid;
        snap.timestamp_ns = steady_now_ns();

        const auto stat = read_proc_stat(pid);
        if (not stat.has_value()) {
            return snap;
        }

        snap.cpu_percent = compute_cpu_percent(pid, stat.value());
        snap.rss_kb = read_rss_kb(pid);
        snap.fd_count = count_file_descriptors(pid);
        snap.thread_count = read_thread_count(pid);
        snap.name = read_comm(pid);
        return snap;
    }

    auto dump_stats_lines() const -> std::vector<std::string> {
        std::lock_guard<std::mutex> lock(mutex_);
        auto lines = std::vector<std::string>{};
        lines.reserve(tracked_.size() + 1);
        lines.emplace_back("[Resource] pid | name | cpu% | rss_kB | fds | threads");
        for (const auto& [pid, snap] : tracked_) {
            lines.push_back(snap.format_line());
        }
        return lines;
    }

    void track(int64_t pid) {
        std::lock_guard<std::mutex> lock(mutex_);
        tracked_[pid] = sample(pid);
    }

    void untrack(int64_t pid) {
        std::lock_guard<std::mutex> lock(mutex_);
        tracked_.erase(pid);
    }

    void refresh_all() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [pid, snap] : tracked_) {
            snap = sample(pid);
        }
    }

  private:
    struct CpuTimes {
        uint64_t utime{0};
        uint64_t stime{0};
        uint64_t total_tick{0};
    };

    static auto steady_now_ns() -> int64_t {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    static auto get_clk_tck() -> long {
        static const long val = sysconf(_SC_CLK_TCK);
        return val > 0 ? val : 100;
    }

    static auto read_proc_stat(int64_t pid) -> std::optional<CpuTimes> {
        char path[64]{};
        std::snprintf(path, sizeof(path), "/proc/%ld/stat", static_cast<long>(pid));

        std::ifstream ifs(path);
        if (not ifs.is_open()) {
            return std::nullopt;
        }

        std::string line;
        if (not std::getline(ifs, line)) {
            return std::nullopt;
        }

        const auto fields = split_stat_fields(line);
        if (fields.size() < 15) {
            return std::nullopt;
        }

        auto times = CpuTimes{};
        try {
            times.utime = std::stoull(fields[13]);
            times.stime = std::stoull(fields[14]);
        } catch (...) {
            return std::nullopt;
        }

        FILE* fp = fopen("/proc/stat", "r");
        if (fp == nullptr) {
            times.total_tick = 1;
            return times;
        }
        char buf[256]{};
        if (fgets(buf, sizeof(buf), fp) != nullptr) {
            const auto parts = split_whitespace(buf);
            if (parts.size() >= 8) {
                uint64_t sum = 0;
                for (size_t i = 1; i <= 7 && i < parts.size(); ++i) {
                    try { sum += std::stoull(parts[i]); } catch (...) { sum += 0; }
                }
                times.total_tick = sum > 0 ? sum : 1;
            } else {
                times.total_tick = 1;
            }
        } else {
            times.total_tick = 1;
        }
        fclose(fp);

        return times;
    }

    static auto compute_cpu_percent(int64_t pid, const CpuTimes& current) -> double {
        std::lock_guard<std::mutex> lock(cpu_cache_mutex());
        auto& prev = cpu_cache()[pid];
        if (prev.total_tick == 0 || current.total_tick == prev.total_tick) {
            prev = current;
            return 0.0;
        }

        const auto d_utime = static_cast<double>(current.utime - prev.utime);
        const auto d_stime = static_cast<double>(current.stime - prev.stime);
        const auto d_total = static_cast<double>(current.total_tick - prev.total_tick);

        prev = current;

        if (d_total <= 0.0) {
            return 0.0;
        }
        return ((d_utime + d_stime) / d_total) * 100.0 * static_cast<double>(get_clk_tck());
    }

    static auto read_rss_kb(int64_t pid) -> uint64_t {
        char path[64]{};
        std::snprintf(path, sizeof(path), "/proc/%ld/status", static_cast<long>(pid));
        std::ifstream ifs(path);
        if (not ifs.is_open()) {
            return 0;
        }
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.compare(0, 6, "VmRSS:") == 0) {
                const auto pos = line.find_first_of("0123456789", 6);
                if (pos != std::string::npos) {
                    try { return std::stoul(line.substr(pos)); } catch (...) { return 0; }
                }
                break;
            }
        }
        return 0;
    }

    static auto count_file_descriptors(int64_t pid) -> int {
        char path[64]{};
        std::snprintf(path, sizeof(path), "/proc/%ld/fd", static_cast<long>(pid));
        auto dir = ::opendir(path);
        if (dir == nullptr) {
            return -1;
        }
        int count = 0;
        while (::readdir(dir) != nullptr) {
            ++count;
        }
        ::closedir(dir);
        return count > 2 ? count - 2 : 0;
    }

    static auto read_thread_count(int64_t pid) -> int64_t {
        char path[64]{};
        std::snprintf(path, sizeof(path), "/proc/%ld/stat", static_cast<long>(pid));
        std::ifstream ifs(path);
        if (not ifs.is_open()) {
            return 0;
        }
        std::string line;
        if (not std::getline(ifs, line)) {
            return 0;
        }
        const auto fields = split_stat_fields(line);
        if (fields.size() >= 20) {
            try { return std::stoll(fields[19]); } catch (...) { return 0; }
        }
        return 0;
    }

    static auto read_comm(int64_t pid) -> std::string {
        char path[64]{};
        std::snprintf(path, sizeof(path), "/proc/%ld/comm", static_cast<long>(pid));
        std::ifstream ifs(path);
        if (not ifs.is_open()) {
            return {};
        }
        std::string name;
        if (std::getline(ifs, name)) {
            if (not name.empty() && name.back() == '\n') {
                name.pop_back();
            }
        }
        return name;
    }

    static auto split_stat_fields(const std::string& line) -> std::vector<std::string> {
        auto out = std::vector<std::string>{};
        std::string cur;
        bool in_paren = false;
        for (const auto ch : line) {
            if (ch == '(') {
                in_paren = true;
                cur.push_back(ch);
            } else if (ch == ')') {
                in_paren = false;
                cur.push_back(ch);
            } else if (ch == ' ' && !in_paren) {
                if (!cur.empty()) {
                    out.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(ch);
            }
        }
        if (!cur.empty()) {
            out.push_back(cur);
        }
        return out;
    }

    static auto split_whitespace(const std::string& s) -> std::vector<std::string> {
        auto out = std::vector<std::string>{};
        std::string cur;
        for (const auto ch : s) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                if (!cur.empty()) {
                    out.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(ch);
            }
        }
        if (!cur.empty()) {
            out.push_back(cur);
        }
        return out;
    }

    mutable std::mutex mutex_;
    std::unordered_map<int64_t, ResourceSnapshot> tracked_;

    static inline std::mutex& cpu_cache_mutex() {
        static std::mutex m;
        return m;
    }

    static inline auto cpu_cache() -> std::unordered_map<int64_t, CpuTimes>& {
        static std::unordered_map<int64_t, CpuTimes> c;
        return c;
    }

    static auto getpid() -> int64_t {
        return static_cast<int64_t>(::getpid());
    }
};

}  // namespace star::runtime::observability

#endif  // __RESOURCE_MONITOR_H__

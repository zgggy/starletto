#include "apps/native_runner/app.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "apps/native_runner/runner_utils.h"
#include "basement/core/build_info.h"
#include "basement/runtime/component/native_component_host.h"
#include "basement/runtime/factory/runtime_factory.h"
#include "basement/runtime/observability/resource_monitor.h"
#include "basement/schema/builtin/observability/worker_health.s.h"
#include "basement/schema/builtin/observability/worker_stats.s.h"
#include "basement/runtime/plan/plan_diagnostics.h"
#include "basement/runtime/plan/plan_loader.h"
#include "basement/runtime/process/path_resolver.h"
#include "basement/runtime/process/process_orchestrator.h"
#include "basement/schema/native/schema_bootstrap.h"
#include "basement/schema/schema_value.h"
#include "basement/transport/shm/shm_channel.h"
#include "basement/transport/shm/shm_routed_message_bus.h"
#include "basement/transport/rpc/rpc.h"
#include "basement/transport/shm/shm_views.h"


namespace star::native_runner {

    auto App::make_timestamp() -> std::string  {
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&t, &tm);
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        return std::string(buf);
    }

    void App::configure_routed_transport_if_needed(star::runtime::RuntimeContext& context, const RunOptions& options) {
        auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context.message_bus());
        if (routed == nullptr) {
            return;
        }
        if (not options.route_plan.empty() || not options.process_group.empty()) {
            routed->configure_route_registry(options.process_group, options.route_plan);
            return;
        }
        routed->configure_route_plan(options.shm_pub_topics, options.shm_sub_topics);
    }

    void App::maybe_dump_transport_stats(const star::transport::MessageBus& message_bus) {
        if (const auto* value = std::getenv("STAR_TRANSPORT_STATS"); value == nullptr || std::string(value) != "1") {
            return;
        }
        for (const auto& line : message_bus.dump_stats_lines()) {
            logI << line;
        }
    }

    auto App::resolve_log_dir(const RunOptions& options) -> std::string  {
        if (not options.log_dir.empty()) {
            return options.log_dir;
        }
        if (not options.log_file.empty()) {
            const auto p = std::filesystem::path(options.log_file);
            if (p.has_parent_path()) {
                return p.parent_path().string();
            }
        }
        return {};
    }

    auto App::resolve_library_key(const star::runtime::plan::ComponentLoadPlan& plan) -> std::string  {
        const auto& lib = plan.component_library.value_or("");
        if (lib.empty()) {
            return {};
        }
        if (const auto validated = star::runtime::validate_declared_library(lib, plan.module_dir.value_or("")); not validated.empty()) {
            return validated;
        }
        if (auto resolved = resolve_component_dir_library(plan, lib)) {
            return *resolved;
        }
        if (auto resolved = resolve_module_dir_library(plan, lib)) {
            return *resolved;
        }
        if (auto resolved = resolve_build_tree_library(plan, lib)) {
            return *resolved;
        }
        if (auto resolved = resolve_env_library(lib)) {
            return *resolved;
        }
        return lib;
    }

    auto App::resolve_component_dir_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string>  {
        const auto& conf_path = plan.component_conf_path.value_or("");
        if (conf_path.empty() || lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        const auto cp = std::filesystem::path(conf_path);
        auto candidate = std::filesystem::weakly_canonical(cp.parent_path() / lib, ec);
        if (ec) {
            return std::nullopt;
        }
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        return std::nullopt;
    }

    auto App::resolve_module_dir_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string>  {
        const auto& module_dir = plan.module_dir.value_or("");
        if (module_dir.empty() || lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        auto candidate = std::filesystem::weakly_canonical(std::filesystem::path(module_dir) / lib, ec);
        if (ec) {
            return std::nullopt;
        }
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        return std::nullopt;
    }

    auto App::split_path_list(const char* value) -> std::vector<std::string>  {
        if (value == nullptr) {
            return {};
        }
        auto s = std::string(value);
        std::vector<std::string> out;
        std::string cur;
        for (const auto ch : s) {
            if (ch == ':') {
                if (not cur.empty()) {
                    out.emplace_back(std::move(cur));
                    cur = {};
                }
                continue;
            }
            cur.push_back(ch);
        }
        if (not cur.empty()) {
            out.emplace_back(std::move(cur));
        }
        return out;
    }

    auto App::resolve_env_library(const std::string& lib) -> std::optional<std::string>  {
        if (lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        const auto search_paths = split_path_list(std::getenv("STAR_LIB_PATH"));
        for (const auto& dir : search_paths) {
            if (dir.empty()) {
                continue;
            }
            const auto base = std::filesystem::path(dir);
            const auto candidate = std::filesystem::weakly_canonical(base / lib, ec);
            if (ec) {
                continue;
            }
            if (std::filesystem::exists(candidate, ec) && !ec) {
                return candidate.string();
            }
        }
        return std::nullopt;
    }

    auto App::resolve_build_tree_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string>  {
        const auto& conf_path = plan.component_conf_path.value_or("");
        if (conf_path.empty() || lib.empty()) {
            return std::nullopt;
        }
        std::error_code ec;
        const auto cp = std::filesystem::path(conf_path);
        auto root = star::runtime::process::find_project_root(cp);
        if (not root) {
            return std::nullopt;
        }
        auto component_dir = std::filesystem::weakly_canonical(cp.parent_path(), ec);
        if (ec) {
            return std::nullopt;
        }
        const auto rel = std::filesystem::relative(component_dir, *root, ec);
        if (ec) {
            return std::nullopt;
        }
        auto candidate = std::filesystem::weakly_canonical((*root / "build" / rel / lib), ec);
        if (ec) {
            return std::nullopt;
        }
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        return std::nullopt;
    }

}  // namespace star::native_runner

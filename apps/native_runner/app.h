#ifndef __NATIVE_RUNNER_APP_H__
#define __NATIVE_RUNNER_APP_H__

#include <optional>
#include <string>
#include <vector>

#include "apps/native_runner/types.h"
#include "basement/runtime/plan/plan_builder.h"

namespace star::runtime {
class RuntimeContext;
}  // namespace star::runtime
namespace star::transport {
class MessageBus;
}  // namespace star::transport

namespace star::native_runner {

class App {
  public:
    auto run(const RunOptions& options, const std::vector<std::string>& component_conf_paths) -> int;
    auto run_multi_process(const RunOptions& options, const std::vector<std::string>& component_conf_paths) -> int;
    auto list(const std::vector<std::string>& module_conf_paths) -> int;
    auto graph_modules_dot(const std::vector<std::string>& module_conf_paths) -> int;
    auto graph_components_dot(const std::vector<std::string>& module_conf_paths) -> int;
    auto doctor(const std::vector<std::string>& module_conf_paths, bool deep) -> int;
    auto selftest_transport(RunOptions options) -> int;
    auto monitor(const RunOptions& options, [[maybe_unused]] const std::vector<std::string>& component_conf_paths) -> int;
  public:
    static auto make_timestamp() -> std::string;
  private:
    static auto selftest_shm_channel_multiprocess() -> int;
    static void configure_routed_transport_if_needed(star::runtime::RuntimeContext& context, const RunOptions& options);
    static void maybe_dump_transport_stats(const star::transport::MessageBus& message_bus);
    static auto resolve_log_dir(const RunOptions& options) -> std::string;
    static auto resolve_library_key(const star::runtime::plan::ComponentLoadPlan& plan) -> std::string;
    static auto resolve_component_dir_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string>;
    static auto resolve_module_dir_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string>;
    static auto split_path_list(const char* value) -> std::vector<std::string>;
    static auto resolve_env_library(const std::string& lib) -> std::optional<std::string>;
    static auto resolve_build_tree_library(const star::runtime::plan::ComponentLoadPlan& plan, const std::string& lib) -> std::optional<std::string>;
};

}  // namespace star::native_runner

#endif  // __NATIVE_RUNNER_APP_H__

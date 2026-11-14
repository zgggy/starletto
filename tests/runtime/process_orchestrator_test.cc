#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "basement/runtime/process/process_orchestrator.h"

#include "tests/support/star_test.h"

namespace {

using star::runtime::process::ProcessOrchestrator;
using star::runtime::process::WorkerLaunchConfig;

TEST_CASE("ProcessOrchestrator builds a deterministic route plan") {
    const auto orchestrator = ProcessOrchestrator{""};

    const auto producers = std::unordered_map<std::string, std::string>{
        {"planning.trajectory", "planning"},
        {"setting.snapshot", "setting"},
    };
    // 单消费者用例：条目格式确定（channel|producer|consumer），条目间按字典序拼接
    const auto consumers = std::unordered_map<std::string, std::unordered_set<std::string>>{
        {"planning.trajectory", {"control"}},
        {"setting.snapshot", {"debug"}},
    };

    const auto plan = orchestrator.build_route_plan(producers, consumers);
    REQUIRE_EQ(plan, std::string("planning.trajectory|planning|control;setting.snapshot|setting|debug"));
}

TEST_CASE("ProcessOrchestrator includes every cross-group consumer regardless of order") {
    const auto orchestrator = ProcessOrchestrator{""};

    const auto plan = orchestrator.build_route_plan(
        {{"planning.trajectory", "planning"}},
        {{"planning.trajectory", {"control", "debug"}}});
    // 消费者来自 unordered_set，条目内顺序不保证；断言只看集合内容
    CHECK(plan.find("planning.trajectory|planning|") == 0);
    CHECK(plan.find("control") != std::string::npos);
    CHECK(plan.find("debug") != std::string::npos);
}

TEST_CASE("ProcessOrchestrator skips channels with no producer") {
    const auto orchestrator = ProcessOrchestrator{""};

    const auto plan = orchestrator.build_route_plan(
        {},
        {{"orphan.channel", {"someone"}}});
    REQUIRE(plan.empty());
}

TEST_CASE("ProcessOrchestrator skips channels consumed inside the producer group") {
    const auto orchestrator = ProcessOrchestrator{""};

    const auto plan = orchestrator.build_route_plan(
        {{"inproc.channel", "planning"}},
        {{"inproc.channel", {"planning", "other"}}});
    // 生产者自身也是消费者：同组走 inproc，不进入 shm 计划
    REQUIRE(plan.empty());
}

// 真实 fork/exec：注入 /bin/true 作为 worker 二进制
TEST_CASE("ProcessOrchestrator launches a worker and reports a zero exit") {
    const auto orchestrator = ProcessOrchestrator{"/usr/bin/true"};

    auto config = WorkerLaunchConfig{};
    config.group = "smoke";
    auto launched = orchestrator.launch_worker(config);
    REQUIRE(launched.pid > 0);
    if (launched.ready_fd >= 0) {
        ::close(launched.ready_fd);
    }

    const auto pid = launched.pid;
    const auto result = orchestrator.wait_all({pid}, {{pid, "smoke"}});
    REQUIRE(result.ok);
    REQUIRE_EQ(result.exited_workers.size(), 1U);
    REQUIRE_EQ(result.exited_workers.front().first, std::string("smoke"));
    REQUIRE_EQ(result.exited_workers.front().second, 0);
}

TEST_CASE("ProcessOrchestrator surfaces a non-zero worker exit") {
    const auto orchestrator = ProcessOrchestrator{"/usr/bin/false"};

    auto config = WorkerLaunchConfig{};
    config.group = "broken";
    auto launched = orchestrator.launch_worker(config);
    REQUIRE(launched.pid > 0);
    if (launched.ready_fd >= 0) {
        ::close(launched.ready_fd);
    }

    const auto pid = launched.pid;
    const auto result = orchestrator.wait_all({pid}, {{pid, "broken"}});
    REQUIRE(not result.ok);
    REQUIRE_EQ(result.exited_workers.front().second, 1);
}

TEST_CASE("ProcessOrchestrator aggregates multiple workers") {
    const auto orchestrator = ProcessOrchestrator{"/usr/bin/true"};

    auto config = WorkerLaunchConfig{};
    config.group = "a";
    auto launched_a = orchestrator.launch_worker(config);
    config.group = "b";
    auto launched_b = orchestrator.launch_worker(config);
    REQUIRE(launched_a.pid > 0);
    REQUIRE(launched_b.pid > 0);
    if (launched_a.ready_fd >= 0) { ::close(launched_a.ready_fd); }
    if (launched_b.ready_fd >= 0) { ::close(launched_b.ready_fd); }
    const auto pid_a = launched_a.pid;
    const auto pid_b = launched_b.pid;

    const auto result = orchestrator.wait_all({pid_a, pid_b}, {{pid_a, "a"}, {pid_b, "b"}});
    REQUIRE(result.ok);
    REQUIRE_EQ(result.exited_workers.size(), 2U);
}

}  // namespace

TEST_CASE("restart backoff grows exponentially and caps") {
    REQUIRE_EQ(star::runtime::process::restart_delay_ms(0, 500), int64_t{500});
    REQUIRE_EQ(star::runtime::process::restart_delay_ms(1, 500), int64_t{1000});
    REQUIRE_EQ(star::runtime::process::restart_delay_ms(2, 500), int64_t{2000});
    REQUIRE_EQ(star::runtime::process::restart_delay_ms(10, 500), int64_t{16000});
}

TEST_CASE("wait_ready reports a worker that writes the ready byte") {
    int fds[2]{};
    REQUIRE(::pipe(fds) == 0);
    const auto pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::close(fds[0]);
        const char ready = 'R';
        (void)::write(fds[1], &ready, 1);
        ::close(fds[1]);
        ::_exit(0);
    }
    ::close(fds[1]);
    auto worker = star::runtime::process::LaunchedWorker{pid, fds[0]};
    REQUIRE(star::runtime::process::ProcessOrchestrator::wait_ready(worker, 1000));
    int status = 0;
    ::waitpid(pid, &status, 0);
}

TEST_CASE("wait_ready reports failure when the worker exits without ready") {
    int fds[2]{};
    REQUIRE(::pipe(fds) == 0);
    const auto pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::close(fds[0]);
        ::_exit(0);
    }
    ::close(fds[1]);
    auto worker = star::runtime::process::LaunchedWorker{pid, fds[0]};
    REQUIRE(not star::runtime::process::ProcessOrchestrator::wait_ready(worker, 1000));
    int status = 0;
    ::waitpid(pid, &status, 0);
}

TEST_CASE("wait_ready times out when the worker neither writes nor exits") {
    int fds[2]{};
    REQUIRE(::pipe(fds) == 0);
    const auto pid = ::fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        ::close(fds[0]);
        ::sleep(1);
        ::_exit(0);
    }
    ::close(fds[1]);
    auto worker = star::runtime::process::LaunchedWorker{pid, fds[0]};
    REQUIRE(not star::runtime::process::ProcessOrchestrator::wait_ready(worker, 50));
    ::kill(pid, SIGKILL);
    int status = 0;
    ::waitpid(pid, &status, 0);
}

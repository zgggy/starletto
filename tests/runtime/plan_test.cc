#include <memory>
#include <string>

#include "basement/runtime/factory/runtime_factory.h"
#include "basement/runtime/plan/plan_builder.h"
#include "basement/transport/shm/shm_service_bus.h"

#include "tests/support/star_test.h"

namespace {

using star::generated::runtime::component_load_plan::ComponentLoadPlan;
using star::generated::runtime::component_load_plan::ServicePortPlan;

auto make_service_plan(const std::string& group, const std::string& service_channel) -> ComponentLoadPlan {
    auto plan = ComponentLoadPlan{};
    plan.module_name = "test";
    plan.component_name = "svc_provider";
    plan.component_class = "test::SvcProvider";
    plan.process_group = group;

    auto service = ServicePortPlan{};
    service.name = "get";
    service.channel = service_channel;
    service.role = std::string("server");
    plan.services = std::vector<ServicePortPlan>{service};
    return plan;
}

// P1-1：服务端口必须折算成 <name>/request 与 <name>/response 两条通道，
// 否则服务通道永远不进路由计划，跨进程 RPC 会静默超时
TEST_CASE("build_process_plan folds service ports into channel lists") {
    const auto plan = make_service_plan("setting", "setting.parameter.get");
    const auto process_plan = star::runtime::plan::build_process_plan({plan});

    REQUIRE(process_plan.groups.has_value());
    REQUIRE_EQ(process_plan.groups->size(), 1U);
    const auto& group = process_plan.groups->front();

    auto contains = [](const std::vector<std::string>& values, const std::string& needle) {
        for (const auto& v : values) {
            if (v == needle) {
                return true;
            }
        }
        return false;
    };

    // server：消费 request、生产 response
    REQUIRE(group.consumed_channels.has_value());
    REQUIRE(contains(*group.consumed_channels, "setting.parameter.get/request"));
    REQUIRE(group.produced_channels.has_value());
    REQUIRE(contains(*group.produced_channels, "setting.parameter.get/response"));
}

// P1-1：shm_routed 下的服务总线必须是 ShmServiceBus，
// 否则客户端与服务端不在同一进程时 call() 直接报 unavailable
TEST_CASE("RuntimeFactory selects ShmServiceBus for shm_routed transport") {
    const auto context = star::runtime::RuntimeFactory::create_native_context("shm_routed");
    REQUIRE(context != nullptr);
    REQUIRE(dynamic_cast<star::transport::ShmServiceBus*>(&context->service_bus()) != nullptr);
}

TEST_CASE("RuntimeFactory keeps NativeServiceBus for the default transport") {
    const auto context = star::runtime::RuntimeFactory::create_native_context();
    REQUIRE(context != nullptr);
    REQUIRE(dynamic_cast<star::transport::ShmServiceBus*>(&context->service_bus()) == nullptr);
}

}  // namespace

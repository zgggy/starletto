#include <memory>
#include <string>
#include <vector>

#include "basement/runtime/component/component.h"
#include "basement/schema/schema_value.h"
#include "basement/runtime/factory/runtime_factory.h"
#include "basement/runtime/plan/plan_builder.h"
#include "basement/transport/rpc/rpc.h"

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

// 决策四：服务并入通道——commands 块创建的是普通通道上的 RpcServer，
// 客户端用同一个 MessageBus 直接调，不再有第二套服务总线
class ServiceEchoProbe final : public star::runtime::Component {
  public:
    auto handle_service_request(const std::string&, const star::transport::ServiceRequest& request)
        -> star::transport::SerializedServiceResponse override {
        auto bytes = std::vector<uint8_t>{};
        if (const auto binary = request.body.payload.as<star::transport::BinaryPayloadView>()) {
            bytes.assign(binary->data, binary->data + binary->size);
        }
        auto response = star::transport::ServiceResponse{};
        response.ok = true;
        auto owner = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
        response.body.payload = star::base::TypedConstPtr::from(owner);
        auto serializer = star::transport::MessageSerializer{};
        serializer.payload_size_hint = owner->size();
        serializer.encode_binary = [owner]() { return *owner; };
        return star::transport::SerializedServiceResponse{response, std::move(serializer)};
    }
};

TEST_CASE("a service declared in the plan is callable over ordinary channels") {
    const auto context = star::runtime::RuntimeFactory::create_native_context();
    REQUIRE(context != nullptr);

    auto probe = ServiceEchoProbe{};
    probe.set_context(context);
    probe.set_load_plan(make_service_plan("setting", "test.rpc.plan.service"));
    probe.auto_configure();

    auto client = star::transport::RpcClient{context->message_bus(), "test.rpc.plan.service"};
    REQUIRE(client.available());

    const auto payload = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{0x11, 0x22, 0x33});
    auto request = star::transport::ServiceRequest{};
    request.body.payload = star::base::TypedConstPtr::from(payload);
    auto serializer = star::transport::MessageSerializer{};
    serializer.encode_binary = [payload]() { return *payload; };

    const auto response = client.call_serialized(star::transport::SerializedServiceRequest{request, serializer});
    REQUIRE(response.ok);
    const auto binary = response.body.payload.as<star::transport::BinaryPayloadView>();
    REQUIRE(binary != nullptr);
    REQUIRE_EQ(std::vector<uint8_t>(binary->data, binary->data + binary->size), (std::vector<uint8_t>{0x11, 0x22, 0x33}));
}

}  // namespace

// P2-5：module.conf 的 logging.level 必须能解析并进入 plan，供 host 设置 per-module 级别
TEST_CASE("module.conf logging level is parsed into the plan") {
    using star::generated::manifest::module::ModuleConf;

    const auto value = star::schema::SchemaParser{"module { name = planning } logging { level = DEBUG }"}.parse();
    const auto conf = ModuleConf::from_value(value);
    REQUIRE(conf.logging.has_value());
    REQUIRE(conf.logging->level.has_value());
    REQUIRE_EQ(*conf.logging->level, std::string("DEBUG"));

    auto ctx = star::runtime::plan::LoadContext{};
    ctx.module_name = "planning";
    ctx.log_level = "DEBUG";

    auto component_conf = star::generated::manifest::component::ComponentConf{};
    component_conf.component.name = "motion";
    component_conf.component.type = "planning";
    component_conf.component.component_class = "star::planning::Motion";
    const auto plan = star::runtime::plan::build_component_load_plan(component_conf, ctx);
    REQUIRE(plan.log_level.has_value());
    REQUIRE_EQ(*plan.log_level, std::string("DEBUG"));
}

// HealthChecker 接线的配置来源：component.conf 的 health 块必须进 plan
TEST_CASE("component health block flows into the load plan") {
    using star::generated::manifest::component::HealthConf;

    auto health = HealthConf{};
    health.channel = std::string("planning.trajectory");
    health.min_hz = 10.0;
    health.window_ms = int64_t{1500};
    health.max_cpu_percent = 75.0;
    health.max_rss_mb = int64_t{256};
    health.max_fd_count = int64_t{128};

    auto component_conf = star::generated::manifest::component::ComponentConf{};
    component_conf.component.name = "motion";
    component_conf.component.type = "planning";
    component_conf.component.component_class = "star::planning::Motion";
    component_conf.health = std::move(health);

    auto ctx = star::runtime::plan::LoadContext{};
    ctx.module_name = "planning";
    const auto plan = star::runtime::plan::build_component_load_plan(component_conf, ctx);
    REQUIRE(plan.health.has_value());
    REQUIRE_EQ(plan.health->channel.value_or(""), std::string("planning.trajectory"));
    REQUIRE_EQ(plan.health->min_hz.value_or(0.0), 10.0);
    REQUIRE_EQ(plan.health->window_ms.value_or(0), int64_t{1500});
    REQUIRE_EQ(plan.health->max_cpu_percent.value_or(0.0), 75.0);
    REQUIRE_EQ(plan.health->max_rss_mb.value_or(0), int64_t{256});
    REQUIRE_EQ(plan.health->max_fd_count.value_or(0), int64_t{128});
}

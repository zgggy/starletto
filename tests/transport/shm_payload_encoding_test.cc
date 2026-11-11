#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/schema/fixed_layout_codec.h"
#include "basement/transport/shm/shm_message_bus.h"
#include "basement/transport/shm/shm_views.h"

#include "tests/support/star_test.h"

namespace {

using LoadPlan = star::generated::runtime::component_load_plan::ComponentLoadPlan;
using star::base::TypedConstPtr;
using star::transport::ChannelDescriptor;
using star::transport::MessageSerializer;
using star::transport::MessageView;
using star::transport::SerializedMessage;
using star::transport::ShmMessageBus;
using star::transport::ShmSlotView;
using star::transport::WriteStatus;

auto make_plan() -> LoadPlan {
    auto plan = LoadPlan{};
    plan.module_name = "setting";
    plan.component_name = "parameter";
    plan.component_class = "star::parameter::Parameter";
    return plan;
}

auto make_descriptor(const std::string& name) -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = name;
    descriptor.type_name = "runtime.component_load_plan";
    descriptor.schema_type_id = LoadPlan::schema_type_id();
    return descriptor;
}

auto make_serialized(const std::shared_ptr<const LoadPlan>& plan) -> SerializedMessage {
    auto message = SerializedMessage{};
    message.view.payload = TypedConstPtr::from(plan);
    message.view.schema_type_id = plan->schema_type_id();
    message.serializer = MessageSerializer{};
    message.serializer.payload_size_hint = plan->encode_binary().size();
    // 两个编码器都给：总线必须选可演进的二进制 TLV，而不是单版本的固定布局
    message.serializer.encode_binary = [plan]() { return plan->encode_binary(); };
    message.serializer.encode_into = [plan](uint8_t* buf, size_t cap) -> size_t {
        return plan->encode_fixed_layout_into(buf, cap);
    };
    return message;
}

// shm 写路径必须优先二进制 TLV：固定布局是位置语义、没有字段 id，
// 跨版本读不了；此前固定布局优先，读侧又固定用 from_binary，跨进程必然解错
TEST_CASE("shm write prefers the versionable binary encoding") {
    const auto bus = std::make_shared<ShmMessageBus>();
    const auto descriptor = make_descriptor("test.payload.encoding");

    auto mutex = std::mutex{};
    auto received = std::vector<MessageView>{};
    const auto reader = bus->create_reader(descriptor, [&](const MessageView& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(msg);
    });
    const auto writer = bus->create_writer(descriptor);
    const auto plan = std::make_shared<const LoadPlan>(make_plan());

    const auto result = writer->write_serialized(make_serialized(plan));
    REQUIRE(result.ok());

    for (int i = 0; i < 100 && received.empty(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    MessageView view;
    {
        std::lock_guard<std::mutex> lock(mutex);
        REQUIRE_EQ(received.size(), 1U);
        view = received.front();
    }
    const auto slot = view.payload.as<ShmSlotView>();
    REQUIRE(slot != nullptr);
    REQUIRE(not star::schema::is_fixed_layout_frame(slot->bytes()));

    const auto decoded = LoadPlan::from_binary(slot->bytes());
    REQUIRE_EQ(decoded.module_name, std::string("setting"));
    REQUIRE_EQ(decoded.component_name, std::string("parameter"));
}

// 跨版本通信：同名不同 version/hash 允许通过总线，解码策略由接收端决定
TEST_CASE("shm accepts a same-name message from a different schema version") {
    const auto bus = std::make_shared<ShmMessageBus>();
    auto descriptor = make_descriptor("test.payload.version");

    auto message = make_serialized(std::make_shared<const LoadPlan>(make_plan()));
    message.view.schema_type_id = star::schema::SchemaTypeId{"runtime.component_load_plan", 2, 0x1234};

    const auto writer = bus->create_writer(descriptor);
    REQUIRE(writer->write_serialized(message).ok());

    // 不同名字仍然是配置错误（编程错误 → 异常，不是运行时状态码）
    auto bad = make_serialized(std::make_shared<const LoadPlan>(make_plan()));
    bad.view.schema_type_id = star::schema::SchemaTypeId{"other.Message", 1, 0};
    auto threw = false;
    try {
        (void)writer->write_serialized(bad);
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
}

}  // namespace

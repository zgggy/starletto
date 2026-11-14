#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "basement/runtime/component/component_dl_helper.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/factory/runtime_factory.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/shm/shm_views.h"

#include "tests/support/star_test.h"

namespace {

using star::base::TypedConstPtr;
using star::generated::runtime::component_load_plan::ComponentLoadPlan;
using star::generated::runtime::component_load_plan::PortPlan;
using star::runtime::ComponentDlHelper;
using star::runtime::ComponentRegistry;
using star::runtime::DlHandle;
using star::runtime::RuntimeFactory;
using star::schema::SchemaTypeId;
using star::transport::BinaryPayloadView;
using star::transport::ChannelDescriptor;
using star::transport::MessageView;
using star::transport::SerializedMessage;

#ifndef STAR_CABI_PLUGIN_PATH
#error "STAR_CABI_PLUGIN_PATH must be defined by the build"
#endif
#ifndef STAR_CABI_BAD_PLUGIN_PATH
#error "STAR_CABI_BAD_PLUGIN_PATH must be defined by the build"
#endif

struct Captured {
    std::vector<uint8_t> bytes;
    SchemaTypeId identity;
    int count{0};
};

auto make_channel(const std::string& name, const std::string& type_name) -> ChannelDescriptor {
    auto descriptor = ChannelDescriptor{};
    descriptor.name = name;
    descriptor.type_name = type_name;
    return descriptor;
}

auto make_raw_serialized(std::vector<uint8_t> bytes, const SchemaTypeId& identity) -> SerializedMessage {
    auto owner = std::make_shared<std::vector<uint8_t>>(std::move(bytes));
    auto serialized = SerializedMessage{};
    serialized.view.payload = TypedConstPtr::from(
        std::make_shared<BinaryPayloadView>(BinaryPayloadView{owner, owner->data(), owner->size()}));
    serialized.view.schema_type_id = identity;
    serialized.serializer.payload_size_hint = owner->size();
    serialized.serializer.encode_binary = [owner]() { return *owner; };
    return serialized;
}

auto make_plan(const std::string& plugin_path) -> ComponentLoadPlan {
    auto plan = ComponentLoadPlan{};
    plan.module_name = "cabi";
    plan.component_name = "echo";
    plan.component_class = "star::cabi::Echo";
    const auto path = std::filesystem::path(plugin_path);
    plan.module_dir = path.parent_path().string();
    plan.component_library = path.filename().string();

    auto input = PortPlan{};
    input.name = "in";
    input.channel = "test.cabi.in";
    input.type_name = "test.cabi.input";
    plan.inputs = std::vector<PortPlan>{input};

    auto output = PortPlan{};
    output.name = "out";
    output.channel = "test.cabi.out";
    output.type_name = "test.cabi.echo";
    plan.outputs = std::vector<PortPlan>{output};
    return plan;
}

auto read_u32(const std::vector<uint8_t>& bytes, size_t offset) -> uint32_t {
    return static_cast<uint32_t>(bytes.at(offset)) | (static_cast<uint32_t>(bytes.at(offset + 1)) << 8) |
           (static_cast<uint32_t>(bytes.at(offset + 2)) << 16) | (static_cast<uint32_t>(bytes.at(offset + 3)) << 24);
}

auto read_u64(const std::vector<uint8_t>& bytes, size_t offset) -> uint64_t {
    auto value = uint64_t{0};
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes.at(offset + i)) << (8 * i);
    }
    return value;
}

}  // namespace

// 纯 C 插件：只包含 C ABI 头，由 C 编译器编成 .so；宿主无需工具链指纹即可加载。
TEST_CASE("C ABI plugin loads and exchanges raw bytes") {
    const auto context = RuntimeFactory::create_native_context();
    REQUIRE(context != nullptr);
    auto& bus = context->message_bus();

    auto captured = Captured{};
    const auto reader = bus.create_reader(make_channel("test.cabi.out", "test.cabi.echo"), [&](const MessageView& message) {
        if (const auto binary = message.payload.as<BinaryPayloadView>()) {
            captured.bytes.assign(binary->data, binary->data + binary->size);
            captured.identity = message.schema_type_id;
            ++captured.count;
        }
    });

    const auto plan = make_plan(STAR_CABI_PLUGIN_PATH);
    auto registry = std::make_shared<ComponentRegistry>();
    registry->set_schema_registry(&context->schema_registry());
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto loader = ComponentDlHelper{};
    loader.load_dynamic_library(plan, handles, registry.get());
    REQUIRE(registry->contains("star::cabi::Echo"));

    auto component = registry->create("star::cabi::Echo");
    REQUIRE(component != nullptr);
    component->set_context(context);
    component->set_load_plan(plan);
    component->auto_configure();
    component->init();

    const auto writer = bus.create_writer(make_channel("test.cabi.in", "test.cabi.input"));
    const auto input = std::vector<uint8_t>{1, 2, 3, 4, 5};
    REQUIRE(writer->write_serialized(make_raw_serialized(input, SchemaTypeId{"test.cabi.input", 1, 0x99})).ok());
    REQUIRE(component->process_once());

    REQUIRE_EQ(captured.count, 1);
    REQUIRE_EQ(captured.bytes.size(), 21U + input.size());
    const auto flags = captured.bytes[0];
    REQUIRE_EQ(static_cast<unsigned>(flags), 0x1DU);  // read ok | missing-port error | schema name | init
    REQUIRE_EQ(read_u32(captured.bytes, 1), static_cast<uint32_t>(input.size()));
    REQUIRE_EQ(read_u64(captured.bytes, 5), uint64_t{1});
    REQUIRE_EQ(read_u64(captured.bytes, 13), uint64_t{0x99});
    REQUIRE_EQ(std::vector<uint8_t>(captured.bytes.begin() + 21, captured.bytes.end()), input);

    REQUIRE_EQ(captured.identity.name, std::string("test.cabi.echo"));
    REQUIRE_EQ(captured.identity.version, int64_t{123});
    REQUIRE_EQ(captured.identity.hash, uint64_t{0x1122334455667788ULL});

    component->shutdown();
}

TEST_CASE("C ABI plugin reports no-data through the read result code") {
    const auto context = RuntimeFactory::create_native_context();
    REQUIRE(context != nullptr);
    auto& bus = context->message_bus();

    auto captured = Captured{};
    const auto reader = bus.create_reader(make_channel("test.cabi.out", "test.cabi.echo"), [&](const MessageView& message) {
        if (const auto binary = message.payload.as<BinaryPayloadView>()) {
            captured.bytes.assign(binary->data, binary->data + binary->size);
            ++captured.count;
        }
    });

    const auto plan = make_plan(STAR_CABI_PLUGIN_PATH);
    auto registry = std::make_shared<ComponentRegistry>();
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto loader = ComponentDlHelper{};
    loader.load_dynamic_library(plan, handles, registry.get());

    auto component = registry->create("star::cabi::Echo");
    REQUIRE(component != nullptr);
    component->set_context(context);
    component->set_load_plan(plan);
    component->auto_configure();
    component->init();
    REQUIRE(component->process_once());

    REQUIRE_EQ(captured.count, 1);
    REQUIRE_EQ(captured.bytes.size(), 21U);
    REQUIRE_EQ(static_cast<unsigned>(captured.bytes[0]), 0x14U);  // missing-port error | init
    REQUIRE_EQ(read_u32(captured.bytes, 1), uint32_t{0});
    component->shutdown();
}

TEST_CASE("C ABI loader rejects a mismatched plugin ABI version") {
    const auto plan = make_plan(STAR_CABI_BAD_PLUGIN_PATH);
    auto registry = std::make_shared<ComponentRegistry>();
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto loader = ComponentDlHelper{};

    auto threw = false;
    try {
        loader.load_dynamic_library(plan, handles, registry.get());
    } catch (const star::excep::RuntimeError&) {
        threw = true;
    }
    REQUIRE(threw);
    REQUIRE(not registry->contains("star::cabi::Echo"));
}

// shm 路径：帧头携带身份，读侧拿到 ShmSlotView 后由 C ABI 适配器拷贝成原始字节。
// 共享内存投递是异步的，这里轮询等待输入到位，再检查输出。
TEST_CASE("C ABI plugin exchanges framed bytes over shared memory") {
    const auto context = RuntimeFactory::create_native_context("shm");
    REQUIRE(context != nullptr);
    auto& bus = context->message_bus();

    auto captured = Captured{};
    const auto reader = bus.create_reader(make_channel("test.cabi.shm.out", "test.cabi.echo"), [&](const MessageView& message) {
        if (const auto binary = message.payload.as<BinaryPayloadView>()) {
            captured.bytes.assign(binary->data, binary->data + binary->size);
            captured.identity = message.schema_type_id;
            ++captured.count;
            return;
        }
        if (const auto slot = message.payload.as<star::transport::ShmSlotView>()) {
            const auto bytes = slot->bytes();
            captured.bytes.assign(bytes.begin(), bytes.end());
            captured.identity = message.schema_type_id;
            ++captured.count;
        }
    });

    auto plan = make_plan(STAR_CABI_PLUGIN_PATH);
    plan.inputs->at(0).channel = "test.cabi.shm.in";
    plan.outputs->at(0).channel = "test.cabi.shm.out";

    auto registry = std::make_shared<ComponentRegistry>();
    auto handles = std::unordered_map<std::string, DlHandle>{};
    auto loader = ComponentDlHelper{};
    loader.load_dynamic_library(plan, handles, registry.get());
    auto component = registry->create("star::cabi::Echo");
    REQUIRE(component != nullptr);
    component->set_context(context);
    component->set_load_plan(plan);
    component->auto_configure();
    component->init();

    const auto writer = bus.create_writer(make_channel("test.cabi.shm.in", "test.cabi.input"));
    const auto input = std::vector<uint8_t>{9, 8, 7};
    REQUIRE(writer->write_serialized(make_raw_serialized(input, SchemaTypeId{"test.cabi.input", 1, 0x99})).ok());

    // 输入先经 poller 进入组件；输出也经 poller 回到测试。两侧都轮询等待。
    auto matched = false;
    for (int i = 0; i < 200 && not matched; ++i) {
        REQUIRE(component->process_once());
        for (int wait = 0; wait < 20; ++wait) {
            if (captured.count > 0) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (captured.count > 0 && (captured.bytes[0] & 0x01U) != 0) {
            matched = true;
        }
    }
    REQUIRE(matched);
    REQUIRE_EQ(captured.bytes.size(), 21U + input.size());
    REQUIRE_EQ(read_u32(captured.bytes, 1), static_cast<uint32_t>(input.size()));
    REQUIRE_EQ(read_u64(captured.bytes, 5), uint64_t{1});
    REQUIRE_EQ(read_u64(captured.bytes, 13), uint64_t{0x99});
    REQUIRE_EQ(std::vector<uint8_t>(captured.bytes.begin() + 21, captured.bytes.end()), input);
    REQUIRE_EQ(captured.identity.name, std::string("test.cabi.echo"));
    REQUIRE_EQ(captured.identity.version, int64_t{123});
    component->shutdown();
}

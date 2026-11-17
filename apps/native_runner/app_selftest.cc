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

    auto App::selftest_transport(RunOptions options) -> int  {
        if (options.transport_name.empty()) {
            options.transport_name = "shm_routed";
        }
        auto context = star::runtime::RuntimeFactory::create_native_context(options.transport_name);

        const auto channel = "/starletto/selftest/transport/routed_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto mode_channel = "/starletto/selftest/transport/mode_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto degrade_channel = "/starletto/selftest/transport/degrade_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        options.shm_pub_topics = channel + "," + mode_channel + "," + degrade_channel;
        options.shm_sub_topics = channel + "," + mode_channel + "," + degrade_channel;
        options.route_plan.clear();
        options.process_group.clear();
        configure_routed_transport_if_needed(*context, options);

        auto* routed = dynamic_cast<star::transport::ShmRoutedMessageBus*>(&context->message_bus());
        if (routed == nullptr) {
            logE << "selftest_transport requires shm_routed transport, got: " << options.transport_name;
            return 2;
        }

        auto descriptor = star::transport::ChannelDescriptor{};
        descriptor.name = channel;
        descriptor.type_name = "selftest";
        descriptor.schema_type_id = star::schema::SchemaTypeId{};
        descriptor.options = star::transport::ChannelOptions{};

        auto received = std::atomic<int64_t>{0};
        auto reader = routed->create_reader(descriptor, [&received](const star::transport::MessageView&) { received.fetch_add(1, std::memory_order_relaxed); });
        if (!reader) {
            logE << "selftest_transport failed to create reader";
            return 3;
        }
        auto writer = routed->create_writer(descriptor);
        if (!writer) {
            logE << "selftest_transport failed to create writer";
            return 3;
        }

        const auto payload = std::vector<uint8_t>(static_cast<size_t>(16), static_cast<uint8_t>(0x5A));
        auto serializer = star::transport::MessageSerializer{};
        serializer.payload_size_hint = payload.size();
        serializer.encode_binary = [payload]() -> std::vector<uint8_t> { return payload; };

        auto view = star::transport::MessageView{};
        view.payload = star::base::TypedConstPtr{};
        view.schema_type_id = star::schema::SchemaTypeId{};
        view.correlation_id = star::transport::CorrelationId{};

        auto msg = star::transport::SerializedMessage{};
        msg.view = view;
        msg.serializer = std::move(serializer);

        if (routed->shm_available()) {
            auto mode_desc = star::transport::ChannelDescriptor{};
            mode_desc.name = mode_channel;
            mode_desc.type_name = "selftest";
            mode_desc.schema_type_id = star::schema::SchemaTypeId{};
            mode_desc.options = star::transport::ChannelOptions{};

            auto mode_writer = routed->create_writer(mode_desc);
            if (!mode_writer) {
                logE << "selftest_transport failed to create mode writer";
                return 3;
            }

            auto view_only = star::transport::MessageView{};
            view_only.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(1));
            view_only.schema_type_id = star::schema::SchemaTypeId{};
            view_only.correlation_id = star::transport::CorrelationId{};

            const auto legacy_r = mode_writer->write(view_only);
            if (legacy_r.status != star::transport::WriteStatus::SERIALIZER_REQUIRED || !legacy_r.shm_required || !legacy_r.inproc_ok) {
                logE << "selftest_transport legacy(shm_read_write) write(view) unexpected: status=" << star::transport::write_status_string(legacy_r.status)
                     << ", shm_required=" << legacy_r.shm_required << ", inproc_ok=" << legacy_r.inproc_ok;
                return 4;
            }
            logI << "selftest_transport ok: legacy(shm_read_write) write(view)->SERIALIZER_REQUIRED";

            auto degrade_desc = star::transport::ChannelDescriptor{};
            degrade_desc.name = degrade_channel;
            degrade_desc.type_name = "selftest";
            degrade_desc.schema_type_id = star::schema::SchemaTypeId{};
            degrade_desc.options = star::transport::ChannelOptions{};

            auto degrade_writer = routed->create_writer(degrade_desc);
            if (!degrade_writer) {
                logE << "selftest_transport failed to create degrade writer";
                return 3;
            }

            auto bad = star::transport::SerializedMessage{};
            bad.view.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(2));
            bad.view.schema_type_id = star::schema::SchemaTypeId{};
            bad.view.correlation_id = star::transport::CorrelationId{};
            bad.serializer.payload_size_hint = 0;
            // 单帧超过槽位大小：真正的「这一帧太大」，区别于合法的空编码
            bad.serializer.encode_binary = []() -> std::vector<uint8_t> { return std::vector<uint8_t>(8 * 1024 * 1024, 0xEE); };

            const auto bad_r = degrade_writer->write_serialized(bad);
            if (bad_r.status != star::transport::WriteStatus::ENCODE_FAILED || !bad_r.shm_required || !bad_r.inproc_ok) {
                logE << "selftest_transport degrade write_serialized(bad) unexpected: status=" << star::transport::write_status_string(bad_r.status)
                     << ", shm_required=" << bad_r.shm_required << ", inproc_ok=" << bad_r.inproc_ok;
                return 4;
            }

            // P0-4：单帧编码失败只是「这一帧太大」，不得永久降级整条通道。
            // 旧实现会在此处把通道降级，之后的正常消息永远发不出去。
            auto good = msg;
            good.view.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(3));
            const auto after_bad_r = degrade_writer->write_serialized(good);
            if (after_bad_r.status != star::transport::WriteStatus::OK || !after_bad_r.shm_ok) {
                logE << "selftest_transport encode-failure isolation unexpected: status=" << star::transport::write_status_string(after_bad_r.status)
                     << ", shm_ok=" << after_bad_r.shm_ok << ", err=" << after_bad_r.error_message;
                return 4;
            }
            logI << "selftest_transport ok: encode failure isolated (channel stays healthy)";
        } else {
            logW << "selftest_transport: shm not available, skipping legacy(shm_read_write)/degrade checks";
        }

        const auto kWrites = int64_t{20};
        for (int64_t i = 0; i < kWrites; ++i) {
            const auto r = writer->write_serialized(msg);
            if (!r.ok()) {
                logE << "selftest_transport write_serialized failed: status=" << star::transport::write_status_string(r.status) << ", err=" << r.error_message;
                return 4;
            }
        }

        const auto got = received.load(std::memory_order_relaxed);
        if (got != kWrites) {
            logE << "selftest_transport failed: duplicate delivery detected: expected=" << kWrites << ", got=" << got;
            return 1;
        }
        logI << "selftest_transport ok: routed inproc delivery count=" << got;

        if (routed->shm_available()) {
            const auto route_channel = "/starletto/selftest/transport/route_plan_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            routed->configure_route_registry("planner", route_channel + "|planner|control");

            auto route_desc = star::transport::ChannelDescriptor{};
            route_desc.name = route_channel;
            route_desc.type_name = "selftest";
            route_desc.schema_type_id = star::schema::SchemaTypeId{};
            route_desc.options = star::transport::ChannelOptions{};

            auto route_writer = routed->create_writer(route_desc);
            if (!route_writer) {
                logE << "selftest_transport failed to create route_plan writer";
                return 3;
            }

            auto view_only = star::transport::MessageView{};
            view_only.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(11));
            view_only.schema_type_id = star::schema::SchemaTypeId{};
            view_only.correlation_id = star::transport::CorrelationId{};

            const auto view_r = route_writer->write(view_only);
            if (view_r.status != star::transport::WriteStatus::SERIALIZER_REQUIRED || !view_r.shm_required || !view_r.inproc_ok) {
                logE << "selftest_transport route_plan write(view) unexpected: status=" << star::transport::write_status_string(view_r.status)
                     << ", shm_required=" << view_r.shm_required << ", inproc_ok=" << view_r.inproc_ok;
                return 4;
            }

            auto route_msg = msg;
            route_msg.view.payload = star::base::TypedConstPtr::from(std::make_shared<const int64_t>(22));
            const auto shm_r = route_writer->write_serialized(route_msg);
            if (!shm_r.ok() || !shm_r.shm_ok || !shm_r.shm_required || !shm_r.inproc_ok) {
                logE << "selftest_transport route_plan write_serialized unexpected: status=" << star::transport::write_status_string(shm_r.status)
                     << ", shm_ok=" << shm_r.shm_ok << ", shm_required=" << shm_r.shm_required << ", inproc_ok=" << shm_r.inproc_ok
                     << ", err=" << shm_r.error_message;
                return 4;
            }
            logI << "selftest_transport ok: route_plan write(view)->SERIALIZER_REQUIRED, write_serialized->OK";
        } else {
            logW << "selftest_transport: shm not available, skipping route_plan write semantics check";
        }

        if (routed->shm_available()) {
            const auto service_name = "/starletto/selftest/service_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            const auto req_topic = service_name + "/request";
            const auto resp_topic = service_name + "/response";

            auto server_bus = std::make_shared<star::transport::ShmRoutedMessageBus>();
            auto client_bus = std::make_shared<star::transport::ShmRoutedMessageBus>();
            if (server_bus->shm_available() && client_bus->shm_available()) {
                const auto route_plan = req_topic + "|planner|control;" + resp_topic + "|control|planner";
                server_bus->configure_route_registry("control", route_plan);
                client_bus->configure_route_registry("planner", route_plan);

                // 服务已并入通道：两个独立 routed 总线（模拟两个进程）之间用 RpcServer/RpcClient
                auto server = star::transport::RpcServer{
                    *server_bus,
                    service_name,
                    [](const star::transport::ServiceRequest& req) -> star::transport::SerializedServiceResponse {
                        auto bytes = std::vector<uint8_t>{};
                        if (const auto bin = req.body.payload.as<star::transport::BinaryPayloadView>()) {
                            bytes.assign(bin->data, bin->data + bin->size);
                        } else if (const auto slot = req.body.payload.as<star::transport::ShmSlotView>()) {
                            bytes.assign(slot->data, slot->data + slot->size);
                        }
                        auto resp = star::transport::ServiceResponse{};
                        resp.ok = true;
                        auto payload = std::make_shared<const std::vector<uint8_t>>(bytes);
                        resp.body.payload = star::base::TypedConstPtr::from(payload);
                        auto serializer = star::transport::MessageSerializer{};
                        serializer.payload_size_hint = payload->size();
                        serializer.encode_binary = [payload]() -> std::vector<uint8_t> { return *payload; };
                        return star::transport::SerializedServiceResponse{resp, std::move(serializer)};
                    }};

                auto client = star::transport::RpcClient{*client_bus, service_name, 15000};

                const auto kCalls = int64_t{200};
                for (int64_t i = 0; i < kCalls; ++i) {
                    auto payload = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>(static_cast<size_t>(16), static_cast<uint8_t>(i & 0xFF)));
                    auto req = star::transport::ServiceRequest{};
                    req.body.payload = star::base::TypedConstPtr::from(payload);
                    req.body.correlation_id = star::transport::CorrelationId::generate();

                    auto serializer = star::transport::MessageSerializer{};
                    serializer.payload_size_hint = payload->size();
                    serializer.encode_binary = [payload]() -> std::vector<uint8_t> { return *payload; };

                    const auto r = client.call_serialized(star::transport::SerializedServiceRequest{req, std::move(serializer)});
                    if (!r.ok) {
                        logE << "selftest_transport failed: service call failed: " << r.message;
                        return 5;
                    }
                    if (const auto bin = r.body.payload.as<star::transport::BinaryPayloadView>()) {
                        if (bin->size != payload->size() || std::memcmp(bin->data, payload->data(), payload->size()) != 0) {
                            logE << "selftest_transport failed: service response mismatch";
                            return 5;
                        }
                    } else if (const auto slot = r.body.payload.as<star::transport::ShmSlotView>()) {
                        if (slot->size != payload->size() || std::memcmp(slot->data, payload->data(), payload->size()) != 0) {
                            logE << "selftest_transport failed: service response mismatch";
                            return 5;
                        }
                    } else {
                        logE << "selftest_transport failed: service response payload type unexpected";
                        return 5;
                    }
                }

                logI << "selftest_transport ok: service soak calls=" << kCalls;
            } else {
                logW << "selftest_transport: shm not available for service soak, skipping";
            }
        }

        const auto shm_ok = selftest_shm_channel_multiprocess();
        if (shm_ok != 0) {
            return shm_ok;
        }
        return 0;
    }

    auto App::selftest_shm_channel_multiprocess() -> int  {
        struct Payload {
            uint64_t writer_id{0};
            uint64_t seq{0};
            uint64_t magic{0x5A5A5A5A5A5A5A5AULL};
        };

        static constexpr uint64_t kMagic = 0x5A5A5A5A5A5A5A5AULL;

        // shm 名称长度受限（macOS PSHMNAMLEN=32 含结尾 NUL），必须短且定长；
        // 取 pid 与时间戳散列以避免跨进程/跨次运行重名
        char name_buf[32]{};
        const auto seed = (static_cast<uint64_t>(::getpid()) << 32) ^ static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::snprintf(name_buf, sizeof(name_buf), "/st_selftest_%08lx", static_cast<unsigned long>(seed & 0xffffffffUL));
        const auto name = std::string(name_buf);

        constexpr uint32_t kCapacity = 1024 * 1024;

        constexpr uint64_t kWritesPerWriter = 8000;
        constexpr int64_t kTimeoutMs = 15000;

        const auto spawn_writer = [&](uint64_t writer_id) -> pid_t {
            const auto pid = ::fork();
            if (pid < 0) {
                return -1;
            }
            if (pid == 0) {
                auto ch = std::make_shared<star::transport::ShmChannel>(name, kCapacity);
                for (uint64_t i = 0; i < kWritesPerWriter; ++i) {
                    auto loan = ch->loan(sizeof(Payload));
                    const auto seq = loan.seq;
                    auto payload = Payload{};
                    payload.writer_id = writer_id;
                    payload.seq = seq;
                    payload.magic = kMagic;
                    std::memcpy(loan.data(), &payload, sizeof(payload));
                    loan.commit(sizeof(payload));
                }
                _exit(0);
            }
            return pid;
        };

        const auto w1 = spawn_writer(1);
        const auto w2 = spawn_writer(2);
        if (w1 < 0 || w2 < 0) {
            if (w1 > 0) { ::kill(w1, SIGKILL); }
            if (w2 > 0) { ::kill(w2, SIGKILL); }
            if (w1 > 0) { ::waitpid(w1, nullptr, 0); }
            if (w2 > 0) { ::waitpid(w2, nullptr, 0); }
            logE << "selftest_shm failed: fork failed";
            return 2;
        }

        auto channel = std::make_shared<star::transport::ShmChannel>(name, kCapacity);

        const auto start = std::chrono::steady_clock::now();
        uint64_t last_seq = 0;
        uint64_t checked = 0;
        uint64_t bad = 0;
        uint64_t overwritten = 0;
        const auto slot_count = channel->slot_count();
        bool w1_done = false;
        bool w2_done = false;

        while (true) {
            for (uint64_t i = 0; i < 16; ++i) {
                auto r = channel->read(&last_seq);
                if (!r.has_value()) {
                    break;
                }
                const auto* ptr = channel->payload_ptr(r->slot);
                if (ptr == nullptr || r->size < sizeof(Payload)) {
                    bad += 1;
                    continue;
                }
                auto payload = Payload{};
                std::memcpy(&payload, ptr, sizeof(payload));
                if (payload.magic != kMagic) {
                    bad += 1;
                    continue;
                }
                if (payload.seq != r->seq) {
                    const auto cur = channel->current_seq();
                    if (slot_count > 0 && cur >= r->seq + slot_count) {
                        overwritten += 1;
                        continue;
                    }
                    bad += 1;
                    continue;
                }
                if (payload.writer_id != 1 && payload.writer_id != 2) {
                    bad += 1;
                    continue;
                }
                checked += 1;
            }

            int status = 0;
            if (!w1_done) {
                const auto w = ::waitpid(w1, &status, WNOHANG);
                if (w == w1) {
                    w1_done = true;
                }
            }
            if (!w2_done) {
                const auto w = ::waitpid(w2, &status, WNOHANG);
                if (w == w2) {
                    w2_done = true;
                }
            }
            const auto elapsed_ms = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
            if (w1_done && w2_done) {
                break;
            }
            if (elapsed_ms > kTimeoutMs) {
                ::kill(w1, SIGKILL);
                ::kill(w2, SIGKILL);
                ::waitpid(w1, nullptr, 0);
                ::waitpid(w2, nullptr, 0);
                logE << "selftest_shm failed: timeout waiting for updates";
                return 3;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        if (bad > 0) {
            logE << "selftest_shm failed: detected corrupted reads: bad=" << bad << ", checked=" << checked;
            return 1;
        }
        logI << "selftest_shm ok: checked=" << checked << ", overwritten=" << overwritten;
        return 0;
    }

}  // namespace star::native_runner

#ifndef __STAR_RUNTIME_COMPONENT_COMPONENT_H__
#define __STAR_RUNTIME_COMPONENT_COMPONENT_H__

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "basement/core/common_types.h"
#include "basement/core/logger/logger.h"
#include "basement/runtime/context/frame_context.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/api/service_bus.h"
#include "basement/transport/shm/shm_views.h"

namespace star::runtime {

class Component {
  public:
    virtual ~Component() = default;

    virtual void init() {}
    virtual auto process_once() -> bool { return true; }
    virtual void shutdown() {}

    void set_context(std::shared_ptr<RuntimeContext> context) { context_ = std::move(context); }

    auto context() const -> std::shared_ptr<RuntimeContext> { return context_; }

    void set_load_plan(star::generated::runtime::component_load_plan::ComponentLoadPlan plan) { plan_ = std::move(plan); }

    auto load_plan() const -> const star::generated::runtime::component_load_plan::ComponentLoadPlan* {
        if (not plan_) {
            return nullptr;
        }
        return &(*plan_);
    }

    void auto_configure() {
        if (not plan_ || not context_) {
            return;
        }
        if (not latest_mutex_) {
            latest_mutex_ = std::make_unique<std::mutex>();
        }
        auto& bus = context_->message_bus();
        if (plan_->inputs.has_value()) {
            for (const auto& e : *plan_->inputs) {
                auto d = star::transport::ChannelDescriptor{};
                d.name = e.channel.value_or(e.name);
                d.type_name = e.type_name.value_or("");
                if (e.has_qos_depth()) {
                    d.options.depth = static_cast<uint32_t>(e.qos_depth.value());
                }
                if (e.has_qos_drop_policy()) {
                    const auto& policy_str = e.qos_drop_policy.value();
                    if (policy_str == "drop_newest") {
                        d.options.drop_policy = star::transport::DropPolicy::DROP_NEWEST;
                    }
                }
                auto port_name = e.name;
                auto reader = bus.create_reader(d, [this, port_name](const star::transport::MessageView& msg) {
                    std::lock_guard<std::mutex> lock(*latest_mutex_);
                    latest_messages_[port_name] = msg;
                });
                readers_[e.name] = std::move(reader);
            }
        }
        if (plan_->outputs.has_value()) {
            for (const auto& e : *plan_->outputs) {
                auto d = star::transport::ChannelDescriptor{};
                d.name = e.channel.value_or(e.name);
                d.type_name = e.type_name.value_or("");
                writers_[e.name] = bus.create_writer(d);
                if (e.record.value_or(false)) {
                    if (auto* recordable = context_->recordable_bus()) {
                        recordable->enable_record_channel(d.name);
                    }
                }
                if (e.has_direction() && e.direction.value_or("") == "bidirectional") {
                    auto port_name = e.name;
                    auto bidi_reader = bus.create_reader(d, [this, port_name](const star::transport::MessageView& msg) {
                        std::lock_guard<std::mutex> lock(*latest_mutex_);
                        latest_messages_[port_name] = msg;
                    });
                    readers_[e.name] = std::move(bidi_reader);
                    bidirectional_channels_.insert(e.name);
                }
            }
        }
        if (plan_->services.has_value()) {
            for (const auto& s : *plan_->services) {
                auto desc = star::transport::ServiceDescriptor{};
                desc.name = s.channel.value_or(s.name);
                if (s.has_role() && s.role.value_or("") == "server") {
                    auto server = context_->service_bus().create_server(
                        desc, [this, svc_name = s.name](const star::transport::ServiceRequest& req) -> star::transport::SerializedServiceResponse {
                            return handle_service_request(svc_name, req);
                        });
                    service_servers_[s.name] = std::move(server);
                } else {
                    auto client = context_->service_bus().create_client(desc);
                    service_clients_[s.name] = std::move(client);
                }
            }
        }
    }

    template <typename _Msg>
    auto read_latest(const std::string& port_name) -> std::shared_ptr<const _Msg> {
        if (not latest_mutex_) {
            return nullptr;
        }
        star::transport::MessageView view;
        {
            std::lock_guard<std::mutex> lock(*latest_mutex_);
            const auto it = latest_messages_.find(port_name);
            if (it == latest_messages_.end()) {
                return nullptr;
            }
            view = it->second;
        }
        return decode_message_shared<_Msg>(view);
    }

    template <typename _MsgView>
    auto peek_view(const std::string& port_name) -> std::optional<_MsgView> {
        if (not latest_mutex_) {
            return std::nullopt;
        }
        star::transport::MessageView view;
        {
            std::lock_guard<std::mutex> lock(*latest_mutex_);
            const auto it = latest_messages_.find(port_name);
            if (it == latest_messages_.end()) {
                return std::nullopt;
            }
            view = it->second;
        }
        if (const auto slot = view.payload.as<star::transport::ShmSlotView>()) {
            if (slot->size > 0) {
                auto v = _MsgView(slot->data, slot->size);
                if (v.valid()) {
                    return v;
                }
            }
        }
        return std::nullopt;
    }

    // 注：原 read_history(port_name, max_count) 已移除——它忽略 max_count 只返回最新一条，
    // 语义与签名不符；真正的历史读取需要 MessageBus 提供按序号回溯的接口后再实现。

    template <typename _Msg>
    void declare_reader(const std::string& name, std::function<void(std::shared_ptr<const _Msg>)> callback) {
        auto d = star::transport::ChannelDescriptor{};
        d.name = resolve_channel(name, true);
        d.type_name = resolve_type_name<_Msg>(name, true);
        auto reader = context()->message_bus().create_reader(d, [this, cb = std::move(callback)](const star::transport::MessageView& msg) { decode_and_dispatch<_Msg>(msg, cb); });
        readers_[name] = std::move(reader);
    }

    template <typename _Msg>
    void declare_writer(const std::string& name) {
        auto d = star::transport::ChannelDescriptor{};
        d.name = resolve_channel(name, false);
        d.type_name = resolve_type_name<_Msg>(name, false);
        writers_[name] = context()->message_bus().create_writer(d);
    }

    template <typename _Msg>
    void declare_delivery_writer(const std::string& name) {
        auto d = star::transport::ChannelDescriptor{};
        d.name = resolve_channel(name, false);
        d.type_name = resolve_type_name<_Msg>(name, false);
        delivery_writers_[name] = context()->message_bus().create_delivery_writer(d);
    }

    template <typename _Msg>
    auto write_with_delivery(const std::string& name, const _Msg& msg, int64_t timeout_ms = 10) -> star::transport::DeliveryResult {
        auto it = delivery_writers_.find(name);
        if (it != delivery_writers_.end() && it->second) {
            auto msg_ptr = std::make_shared<const _Msg>(msg);
            auto serialized = make_serialized_message(msg_ptr);
            auto future = it->second->write_serialized_with_delivery(serialized, timeout_ms);
            return future.get();
        }
        const auto result = write(name, msg);
        return {result.ok(), result.ok() ? "fallback (no delivery writer)" : result.error_message, 0};
    }

    // 错误处理约定：**运行时状况**（transport 失败、编码失败、SHM 降级）以 WriteResult
    // 状态码返回，由调用方决定重试/降级/丢弃；**编程错误**（端口未声明、类型不符）
    // 属于装配缺陷，仍走异常。二者不可混用——把状态码再抛成异常会让调用方无法
    // 用统一方式判断成败，最终又被调度层降级为日志。
    template <typename _Msg>
    auto write(const std::string& name, const _Msg& msg) -> star::transport::WriteResult {
        auto it = writers_.find(name);
        if (it == writers_.end() || not it->second) {
            throw star::excep::RuntimeError("Component write failed, writer not declared: ", name);
        }
        auto msg_ptr = std::make_shared<const _Msg>(msg);
        return it->second->write_serialized(make_serialized_message(msg_ptr));
    }

    template <typename _Msg>
    auto read_channel(const std::string& name) -> std::shared_ptr<const _Msg> {
        return read_latest<_Msg>(name);
    }

    template <typename _Msg>
    auto write_channel(const std::string& name, const _Msg& msg) -> star::transport::WriteResult {
        return write(name, msg);
    }

    template <typename _Req, typename _Res>
    auto call_service(const std::string& name, const _Req& request) -> _Res {
        auto it = service_clients_.find(name);
        std::shared_ptr<star::transport::ServiceClient> client;
        if (it != service_clients_.end() && it->second) {
            client = it->second;
        } else {
            auto desc = star::transport::ServiceDescriptor{name};
            client = context_->service_bus().create_client(desc);
        }
        auto req_ptr = std::make_shared<const _Req>(request);
        auto response = client->call_serialized(make_serialized_service_request(req_ptr));
        if (not response.ok) {
            throw star::excep::RuntimeError("service call failed: ", response.message);
        }
        return decode_message_value<_Res>(response.body, "service response decode failed");
    }

    auto call_service_raw(const std::string& name, const star::transport::ServiceRequest& request) -> star::transport::ServiceResponse {
        auto it = service_clients_.find(name);
        std::shared_ptr<star::transport::ServiceClient> client;
        if (it != service_clients_.end() && it->second) {
            client = it->second;
        } else {
            auto desc = star::transport::ServiceDescriptor{name};
            client = context_->service_bus().create_client(desc);
        }
        return client->call(request);
    }

    template <typename _Req, typename _Res>
    auto async_call_service(const std::string& name, const _Req& request) -> std::future<_Res> {
        auto it = service_clients_.find(name);
        std::shared_ptr<star::transport::ServiceClient> client;
        if (it != service_clients_.end() && it->second) {
            client = it->second;
        } else {
            auto desc = star::transport::ServiceDescriptor{name};
            client = context_->service_bus().create_client(desc);
        }
        auto req_ptr = std::make_shared<const _Req>(request);
        return std::async(std::launch::async, [client, request = make_serialized_service_request(req_ptr)]() mutable -> _Res {
            auto response = client->call_serialized(request);
            if (not response.ok) {
                throw star::excep::RuntimeError("async service call failed: ", response.message);
            }
            if (auto typed = response.body.payload.template as<_Res>()) {
                return *typed;
            }
            if constexpr (has_from_binary<_Res>::value) {
                if (const auto slot = response.body.payload.template as<star::transport::ShmSlotView>()) {
                    if (slot->size > 0) {
                        return _Res::from_binary(slot->bytes());
                    }
                }
                if (const auto binary = response.body.payload.template as<star::transport::BinaryPayloadView>()) {
                    if (binary->size > 0) {
                        return _Res::from_binary(binary->bytes());
                    }
                }
            }
            throw star::excep::RuntimeError("async service response decode failed");
        });
    }

  protected:
    virtual auto handle_service_request(const std::string&, const star::transport::ServiceRequest&) -> star::transport::SerializedServiceResponse {
        auto response = star::transport::ServiceResponse{};
        response.ok = false;
        response.message = "not implemented";
        response.body.correlation_id = star::transport::CorrelationId{};
        return star::transport::SerializedServiceResponse{response, {}};
    }

    auto resolve_channel(const std::string& port_name, bool is_reader) const -> std::string {
        if (const auto* plan = load_plan()) {
            const auto& endpoints = is_reader ? plan->inputs : plan->outputs;
            if (endpoints.has_value()) {
                for (const auto& e : *endpoints) {
                    if (e.name == port_name && e.channel.has_value() && !e.channel->empty()) {
                        return *e.channel;
                    }
                }
            }
        }
        return port_name;
    }

    template <typename _Msg>
    auto resolve_type_name(const std::string& port_name, bool is_reader) const -> std::string {
        if (const auto* plan = load_plan()) {
            const auto& endpoints = is_reader ? plan->inputs : plan->outputs;
            if (endpoints.has_value()) {
                for (const auto& e : *endpoints) {
                    if (e.name == port_name && e.type_name.has_value() && !e.type_name->empty()) {
                        return *e.type_name;
                    }
                }
            }
        }
        return "";
    }

    template <typename _Snapshot, typename _Target>
    auto decode_snapshot_entry(const std::shared_ptr<const _Snapshot>& snap, const std::string& key, const std::string& expected_schema) const -> std::optional<_Target> {
        if (not snap) {
            return std::nullopt;
        }
        const auto it = snap->entries.find(key);
        if (it == snap->entries.end()) {
            return std::nullopt;
        }
        if (it->second.schema_name != expected_schema) {
            return std::nullopt;
        }
        if (not it->second.payload_binary.has_value()) {
            return std::nullopt;
        }
        try {
            const auto& bytes = *it->second.payload_binary;
            return _Target::from_binary(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()));
        } catch (const std::exception& ex) {
            // 解码失败必须可见：否则与「无消息」不可区分，线上无从定位损坏数据
            logE << "decode_snapshot_entry failed: key=" << key << ", schema=" << expected_schema << ", error=" << ex.what();
            return std::nullopt;
        } catch (...) {
            logE << "decode_snapshot_entry failed: key=" << key << ", schema=" << expected_schema << ", error=unknown";
            return std::nullopt;
        }
    }

  private:
    template <typename _Msg, typename _Cb>
    void decode_and_dispatch(const star::transport::MessageView& msg, _Cb& cb) {
        if (auto typed = decode_message_shared<_Msg>(msg)) {
            cb(typed);
        }
    }

    template <typename _Msg>
    static auto decode_message_shared(const star::transport::MessageView& msg) -> std::shared_ptr<const _Msg> {
        if (auto typed = msg.payload.as<_Msg>()) {
            return typed;
        }
        if constexpr (has_from_binary<_Msg>::value) {
            if (const auto slot = msg.payload.as<star::transport::ShmSlotView>()) {
                if (slot->size > 0) {
                    // 槽位可能在投递后、解码前被写方覆写（4 槽环形缓冲），复核代际
                    if (not slot->stable()) {
                        logE << "decode_message skipped: channel=" << msg.schema_type_id.name << ", source=shm_slot, reason=slot overwritten before decode";
                        return nullptr;
                    }
                    try {
                        return std::make_shared<_Msg>(_Msg::from_binary(slot->bytes()));
                    } catch (const std::exception& ex) {
                        logE << "decode_message failed: channel=" << msg.schema_type_id.name << ", source=shm_slot, size=" << slot->size << ", error=" << ex.what();
                        return nullptr;
                    } catch (...) {
                        logE << "decode_message failed: channel=" << msg.schema_type_id.name << ", source=shm_slot, size=" << slot->size << ", error=unknown";
                        return nullptr;
                    }
                }
            }
            if (const auto binary = msg.payload.as<star::transport::BinaryPayloadView>()) {
                if (binary->size > 0) {
                    try {
                        return std::make_shared<_Msg>(_Msg::from_binary(binary->bytes()));
                    } catch (const std::exception& ex) {
                        logE << "decode_message failed: channel=" << msg.schema_type_id.name << ", source=binary, size=" << binary->size << ", error=" << ex.what();
                        return nullptr;
                    } catch (...) {
                        logE << "decode_message failed: channel=" << msg.schema_type_id.name << ", source=binary, size=" << binary->size << ", error=unknown";
                        return nullptr;
                    }
                }
            }
        }
        return nullptr;
    }

    template <typename _Msg>
    static auto decode_message_value(const star::transport::MessageView& msg, const char* error_message) -> _Msg {
        if (auto typed = decode_message_shared<_Msg>(msg)) {
            return *typed;
        }
        throw star::excep::RuntimeError(error_message);
    }

    template <typename _Msg>
    static auto make_serialized_message(const std::shared_ptr<const _Msg>& msg_ptr) -> star::transport::SerializedMessage {
        auto view = star::transport::MessageView{};
        view.payload = star::base::TypedConstPtr::from(msg_ptr);
        return star::transport::SerializedMessage{view, make_message_serializer(msg_ptr)};
    }

    template <typename _Req>
    static auto make_serialized_service_request(const std::shared_ptr<const _Req>& req_ptr) -> star::transport::SerializedServiceRequest {
        auto request = star::transport::ServiceRequest{};
        request.body.payload = star::base::TypedConstPtr::from(req_ptr);
        request.body.correlation_id = star::transport::CorrelationId::generate();
        return star::transport::SerializedServiceRequest{request, make_message_serializer(req_ptr)};
    }

    template <typename _Msg>
    static auto make_message_serializer(const std::shared_ptr<const _Msg>& msg_ptr) -> star::transport::MessageSerializer {
        auto serializer = star::transport::MessageSerializer{};
        serializer.encode_binary = [msg_ptr]() -> std::vector<uint8_t> { return msg_ptr->encode_binary(); };
        if constexpr (has_encode_fixed_layout_into<_Msg>::value) {
            serializer.encode_into = [msg_ptr](uint8_t* buf, size_t cap) -> size_t { return msg_ptr->encode_fixed_layout_into(buf, cap); };
        }
        return serializer;
    }

    template <typename _T, typename = void>
    struct has_from_binary : std::false_type {};

    template <typename _T>
    struct has_from_binary<_T, std::void_t<decltype(_T::from_binary(std::span<const uint8_t>{}))>> : std::true_type {};

    template <typename _T, typename = void>
    struct has_encode_fixed_layout_into : std::false_type {};

    template <typename _T>
    struct has_encode_fixed_layout_into<_T, std::void_t<decltype(std::declval<const _T&>().encode_fixed_layout_into(std::declval<uint8_t*>(), std::declval<size_t>()))>> : std::true_type {};

    std::shared_ptr<RuntimeContext> context_;
    std::optional<star::generated::runtime::component_load_plan::ComponentLoadPlan> plan_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::Reader>> readers_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::Writer>> writers_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::DeliveryWriter>> delivery_writers_;
    std::unordered_set<std::string> bidirectional_channels_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::ServiceClient>> service_clients_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::ServiceServer>> service_servers_;
    std::shared_ptr<std::mutex> latest_mutex_;
    std::unordered_map<std::string, star::transport::MessageView> latest_messages_;
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_COMPONENT_H__

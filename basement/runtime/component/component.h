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

#include "basement/core/common_types.h"
#include "basement/core/logger/logger.h"
#include "basement/runtime/context/frame_context.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/schema/builtin/runtime/component_load_plan.s.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/api/service_bus.h"
#include "basement/transport/native/native_message_bus.h"
#include "basement/transport/shm/shm_message_bus.h"

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
                    bus.enable_record_channel(d.name);
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
                        desc, [this, svc_name = s.name](const star::transport::ServiceRequest& req) -> star::transport::ServiceResponse { return handle_service_request(svc_name, req); });
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
        if (auto typed = view.data.as<_Msg>()) {
            return typed;
        }
        if constexpr (has_from_binary<_Msg>::value) {
            if (const auto shm = view.data.as<star::transport::ShmBinaryView>()) {
                if (shm->size > 0) {
                    try {
                        return std::make_shared<_Msg>(_Msg::from_binary(shm->bytes()));
                    } catch (...) {
                        return nullptr;
                    }
                }
            }
        }
        return nullptr;
    }

    template <typename _Msg>
    auto read_history(const std::string& port_name, size_t max_count = 1) -> std::vector<std::shared_ptr<const _Msg>> {
        auto channel = resolve_channel(port_name, true);
        auto* native_bus = dynamic_cast<star::transport::NativeMessageBus*>(&context_->message_bus());
        if (not native_bus) {
            return {};
        }
        auto out = std::vector<std::shared_ptr<const _Msg>>{};
        if (auto latest = native_bus->peek_latest(channel)) {
            if (auto typed = latest->data.as<_Msg>()) {
                out.push_back(typed);
            }
        }
        return out;
    }

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
    void declare_ack_writer(const std::string& name, int64_t timeout_ms = 10) {
        auto d = star::transport::ChannelDescriptor{};
        d.name = resolve_channel(name, false);
        d.type_name = resolve_type_name<_Msg>(name, false);
        d.options.ack_timeout_ms = timeout_ms;
        ack_writers_[name] = context()->message_bus().create_ack_writer(d);
    }

    template <typename _Msg>
    auto publish_with_ack(const std::string& name, const _Msg& msg, int64_t timeout_ms = 10) -> star::transport::AckResult {
        auto it = ack_writers_.find(name);
        if (it != ack_writers_.end() && it->second) {
            auto msg_ptr = std::make_shared<_Msg>(msg);
            auto view = star::transport::MessageView{};
            view.data = star::base::TypedConstPtr::from(msg_ptr);
            view.encode_binary = [msg_ptr]() -> std::vector<uint8_t> { return msg_ptr->encode_binary(); };
            auto future = it->second->write_with_ack(view, timeout_ms);
            return future.get();
        }
        write(name, msg);
        return {true, "fallback (no ack writer)", 0};
    }

    template <typename _Msg>
    void write(const std::string& name, const _Msg& msg) {
        auto it = writers_.find(name);
        if (it == writers_.end() || not it->second) {
            logW << "Component write failed, writer not declared: " << name;
            return;
        }
        auto msg_ptr = std::make_shared<_Msg>(msg);
        auto view = star::transport::MessageView{};
        view.data = star::base::TypedConstPtr::from(msg_ptr);
        view.encode_binary = [msg_ptr]() -> std::vector<uint8_t> { return msg_ptr->encode_binary(); };
        it->second->write(view);
    }

    template <typename _Msg>
    auto read_channel(const std::string& name) -> std::shared_ptr<const _Msg> {
        return read_latest<_Msg>(name);
    }

    template <typename _Msg>
    void write_channel(const std::string& name, const _Msg& msg) {
        write(name, msg);
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
        auto req = star::transport::ServiceRequest{};
        auto req_ptr = std::make_shared<const _Req>(request);
        req.data_typed = star::base::TypedConstPtr::from(req_ptr);
        req.encode_binary = [req_ptr]() -> std::vector<uint8_t> { return req_ptr->encode_binary(); };
        req.data = req_ptr->to_value();
        auto response = client->call(req);
        if (not response.ok) {
            throw star::excep::RuntimeError("service call failed: ", response.message);
        }
        if (const auto typed = response.data_typed.as<_Res>()) {
            return *typed;
        }
        if (response.encode_binary) {
            const auto bytes = response.encode_binary();
            return _Res::from_binary(std::span<const uint8_t>(bytes.data(), bytes.size()));
        }
        throw star::excep::RuntimeError("service response decode failed");
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
        auto req = star::transport::ServiceRequest{};
        auto req_ptr = std::make_shared<const _Req>(request);
        req.data_typed = star::base::TypedConstPtr::from(req_ptr);
        req.encode_binary = [req_ptr]() -> std::vector<uint8_t> { return req_ptr->encode_binary(); };
        req.data = req_ptr->to_value();
        auto future_response = client->async_call(req);
        auto shared_future = std::make_shared<std::future<star::transport::ServiceResponse>>(std::move(future_response));
        auto promise = std::make_shared<std::promise<_Res>>();
        auto future_res = promise->get_future();
        std::thread([shared_future, promise]() {
            try {
                auto response = shared_future->get();
                if (not response.ok) {
                    promise->set_exception(std::make_exception_ptr(star::excep::RuntimeError("async service call failed: ", response.message)));
                    return;
                }
                if (const auto typed = response.data_typed.as<_Res>()) {
                    promise->set_value(*typed);
                    return;
                }
                if (response.encode_binary) {
                    const auto bytes = response.encode_binary();
                    promise->set_value(_Res::from_binary(std::span<const uint8_t>(bytes.data(), bytes.size())));
                    return;
                }
                promise->set_exception(std::make_exception_ptr(star::excep::RuntimeError("async service response decode failed")));
            } catch (...) {
                promise->set_exception(std::current_exception());
            }
        }).detach();
        return future_res;
    }

  protected:
    virtual auto handle_service_request(const std::string&, const star::transport::ServiceRequest&) -> star::transport::ServiceResponse {
        auto resp = star::transport::ServiceResponse{};
        resp.ok = false;
        resp.message = "not implemented";
        return resp;
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
        } catch (...) {
            return std::nullopt;
        }
    }

  private:
    template <typename _Msg, typename _Cb>
    void decode_and_dispatch(const star::transport::MessageView& msg, _Cb& cb) {
        if (auto typed = msg.data.as<_Msg>()) {
            cb(typed);
            return;
        }
        if constexpr (has_from_binary<_Msg>::value) {
            if (const auto shm = msg.data.as<star::transport::ShmBinaryView>()) {
                if (shm->size > 0) {
                    try {
                        auto decoded = std::make_shared<_Msg>(_Msg::from_binary(shm->bytes()));
                        cb(decoded);
                    } catch (...) {
                    }
                }
            }
        }
    }

    template <typename _T, typename = void>
    struct has_from_binary : std::false_type {};

    template <typename _T>
    struct has_from_binary<_T, std::void_t<decltype(_T::from_binary(std::span<const uint8_t>{}))>> : std::true_type {};

    std::shared_ptr<RuntimeContext> context_;
    std::optional<star::generated::runtime::component_load_plan::ComponentLoadPlan> plan_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::Reader>> readers_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::Writer>> writers_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::AckWriter>> ack_writers_;
    std::unordered_set<std::string> bidirectional_channels_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::ServiceClient>> service_clients_;
    std::unordered_map<std::string, std::shared_ptr<star::transport::ServiceServer>> service_servers_;
    std::shared_ptr<std::mutex> latest_mutex_;
    std::unordered_map<std::string, star::transport::MessageView> latest_messages_;
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_COMPONENT_COMPONENT_H__

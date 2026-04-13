#ifndef __PARAMETER_COMPONENT_H__
#define __PARAMETER_COMPONENT_H__

#include "basement/core/exception.h"
#include "basement/core/logger/logger.h"
#include "basement/runtime/component/component.h"
#include "basement/runtime/component/component_registry.h"
#include "basement/runtime/context/runtime_context.h"
#include "basement/schema/schema_registry.h"
#include "basement/schema/schema_validator.h"
#include "basement/schema/binary_codec.h"
#include "basement/transport/api/message_bus.h"
#include "basement/transport/api/service_bus.h"
#include "libraries/common/schema/schema_value.h"
#include "modules/setting/parameter/parameter_store_service.h"
#include "modules/setting/schema/planning/motion.s.h"
#include "modules/setting/schema/parameter_snapshot.s.h"
#include "modules/setting/schema/service/parameter_get_request.s.h"
#include "modules/setting/schema/service/parameter_get_response.s.h"
#include "modules/setting/schema/service/parameter_list_request.s.h"
#include "modules/setting/schema/service/parameter_list_response.s.h"
#include "modules/setting/schema/service/parameter_set_request.s.h"
#include "modules/setting/schema/service/parameter_set_response.s.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace star::setting {

class ParameterComponent : public star::runtime::Component {
public:
    void init() override {
        if (not context()) {
            throw star::excep::RuntimeError("ParameterComponent init failed, context is null");
        }
        auto parameter_store = std::shared_ptr<star::transport::ParameterStore>(
            &context()->parameter_store(),
            [](star::transport::ParameterStore*) {});
        store_service_ = std::make_shared<ParameterStoreService>(std::move(parameter_store));

        if (not store_service_) {
            throw star::excep::RuntimeError("ParameterComponent init failed, store_service is null");
        }
        load_from_component_conf_path();

        auto writer_descriptor = star::transport::ChannelDescriptor{};
        writer_descriptor.name = resolve_snapshot_channel_name();
        writer_descriptor.type_name = resolve_snapshot_type_name();
        writer_descriptor.schema_type_id = default_snapshot_schema_type_id();
        writer_ = context()->message_bus().create_writer(writer_descriptor);

        register_parameter_services();
    }

    auto process_once() -> bool override {
        publish_snapshot();
        return static_cast<bool>(store_service_);
    }

private:
    static auto default_snapshot_channel_name() -> std::string {
        return "setting.parameter.snapshot";
    }

    static auto default_snapshot_type_name() -> std::string {
        return "star::generated::setting::parameter::snapshot::ParameterSnapshot";
    }

    static auto default_snapshot_schema_type_id() -> star::schema::SchemaTypeId {
        star::schema::SchemaTypeId out;
        out.name = "star.setting.parameter.snapshot";
        out.version = 1;
        return out;
    }

    static auto default_get_service_name() -> std::string {
        return "setting.parameter.get";
    }

    static auto default_set_service_name() -> std::string {
        return "setting.parameter.set";
    }

    static auto default_list_service_name() -> std::string {
        return "setting.parameter.list";
    }

    auto resolve_snapshot_channel_name() const -> std::string {
        const auto* plan = load_plan();
        if (plan && plan->outputs.has_value()) {
            for (const auto& e : *plan->outputs) {
                if (e.name == "snapshot" && e.channel.has_value() && !e.channel->empty()) {
                    return *e.channel;
                }
            }
        }
        return default_snapshot_channel_name();
    }

    auto resolve_snapshot_type_name() const -> std::string {
        const auto* plan = load_plan();
        if (plan && plan->outputs.has_value()) {
            for (const auto& e : *plan->outputs) {
                if (e.name == "snapshot" && e.type_name.has_value() && !e.type_name->empty()) {
                    return *e.type_name;
                }
            }
        }
        return default_snapshot_type_name();
    }

    void register_parameter_services() {
        get_server_ = context()->service_bus().create_server(
            star::transport::ServiceDescriptor{default_get_service_name()},
            [this](const star::transport::ServiceRequest& request) {
                return handle_get_service(request);
            });
        set_server_ = context()->service_bus().create_server(
            star::transport::ServiceDescriptor{default_set_service_name()},
            [this](const star::transport::ServiceRequest& request) {
                return handle_set_service(request);
            });
        list_server_ = context()->service_bus().create_server(
            star::transport::ServiceDescriptor{default_list_service_name()},
            [this](const star::transport::ServiceRequest& request) {
                return handle_list_service(request);
            });
    }

    auto handle_get_service(const star::transport::ServiceRequest& request) -> star::transport::ServiceResponse {
        try {
            const auto typed_request =
                decode_typed_request<star::setting::service::parameter::get_request::ParameterGetRequest>(request);
            auto response = star::setting::service::parameter::get_response::ParameterGetResponse{};
            response.key = typed_request.key;
            const auto value = store_service_->get(typed_request.key);
            if (const auto payload_binary = encode_schema_known_entry(typed_request.key, value); payload_binary.has_value()) {
                response.schema_name = typed_request.key;
                response.payload_binary = std::move(*payload_binary);
            } else {
                response.debug_text = star::schema::dump_schema_text(value);
            }
            return response_ok_typed(std::move(response));
        } catch (const std::exception& e) {
            return response_error(e.what());
        } catch (...) {
            return response_error("get request failed");
        }
    }

    auto handle_set_service(const star::transport::ServiceRequest& request) -> star::transport::ServiceResponse {
        try {
            const auto typed_request =
                decode_typed_request<star::setting::service::parameter::set_request::ParameterSetRequest>(request);
            auto value = decode_set_request_value(typed_request);
            normalize_entry_if_schema_exists(typed_request.key, &value);
            store_service_->set(typed_request.key, value);
            publish_snapshot();

            auto response = star::setting::service::parameter::set_response::ParameterSetResponse{};
            response.updated = true;
            return response_ok_typed(std::move(response));
        } catch (const std::exception& e) {
            return response_error(e.what());
        } catch (...) {
            return response_error("set request failed");
        }
    }

    auto handle_list_service(const star::transport::ServiceRequest& request) -> star::transport::ServiceResponse {
        try {
            const auto typed_request =
                decode_typed_request<star::setting::service::parameter::list_request::ParameterListRequest>(request);
            auto response = star::setting::service::parameter::list_response::ParameterListResponse{};
            response.keys = store_service_->list(typed_request.prefix.value_or(""));
            return response_ok_typed(std::move(response));
        } catch (const std::exception& e) {
            return response_error(e.what());
        } catch (...) {
            return response_error("list request failed");
        }
    }

    template <typename _RequestT>
    static auto decode_typed_request(const star::transport::ServiceRequest& request) -> _RequestT {
        if (const auto typed = request.data_typed.as<_RequestT>()) {
            return *typed;
        }
        if (request.encode_binary) {
            const auto bytes = request.encode_binary();
            if (not bytes.empty()) {
                return _RequestT::from_binary(std::span<const uint8_t>(bytes.data(), bytes.size()));
            }
        }
        if (request.data.is_object()) {
            return _RequestT::from_value(request.data);
        }
        throw star::excep::RuntimeError("ParameterComponent decode typed service request failed");
    }

    template <typename _ResponseT>
    static auto response_ok_typed(_ResponseT response_value) -> star::transport::ServiceResponse {
        auto response = star::transport::ServiceResponse{};
        response.ok = true;
        auto msg = std::make_shared<const _ResponseT>(std::move(response_value));
        response.data_typed = star::base::TypedConstPtr::from(msg);
        response.schema_type_id = _ResponseT::schema_type_id();
        response.encode_binary = [msg]() { return msg->encode_binary(); };
        return response;
    }

    static auto response_error(std::string message) -> star::transport::ServiceResponse {
        auto response = star::transport::ServiceResponse{};
        response.ok = false;
        response.message = std::move(message);
        return response;
    }

    static auto replace_all(std::string s, char from, char to) -> std::string {
        for (auto& ch : s) {
            if (ch == from) {
                ch = to;
            }
        }
        return s;
    }

    static auto strip_extension(const std::string& path) -> std::string {
        const auto pos = path.rfind('.');
        if (pos == std::string::npos) {
            return path;
        }
        return path.substr(0, pos);
    }

    static auto is_conf_file(const std::filesystem::path& p) -> bool {
        if (not std::filesystem::is_regular_file(p)) {
            return false;
        }
        return p.extension() == ".conf";
    }

    auto resolve_parameter_root_path() const -> std::filesystem::path {
        const auto* plan = load_plan();
        if (plan && plan->config_file_path.has_value() && !plan->config_file_path->empty()) {
            const auto raw = std::filesystem::path(*plan->config_file_path);
            if (raw.is_absolute() && std::filesystem::exists(raw)) {
                return raw;
            }
            std::vector<std::filesystem::path> candidates;
            if (plan->module_config_dir.has_value() && !plan->module_config_dir->empty()) {
                candidates.emplace_back(std::filesystem::path(*plan->module_config_dir) / raw);
            }
            if (plan->module_dir.has_value() && !plan->module_dir->empty()) {
                candidates.emplace_back(std::filesystem::path(*plan->module_dir) / raw);
            }
            if (plan->component_conf_path.has_value() && !plan->component_conf_path->empty()) {
                candidates.emplace_back(std::filesystem::path(*plan->component_conf_path).parent_path() / raw);
            }
            candidates.emplace_back(raw);
            for (const auto& candidate : candidates) {
                if (std::filesystem::exists(candidate)) {
                    return std::filesystem::absolute(candidate);
                }
            }
            return std::filesystem::absolute(candidates.front());
        }
        return {};
    }

    void load_from_component_conf_path() {
        const auto root = resolve_parameter_root_path();
        if (root.empty()) {
            return;
        }
        if (not std::filesystem::exists(root)) {
            throw star::excep::RuntimeError("ParameterComponent load config failed, path not exists: ", root.string());
        }

        size_t loaded = 0;
        if (std::filesystem::is_directory(root)) {
            for (const auto& it : std::filesystem::recursive_directory_iterator(root)) {
                if (not is_conf_file(it.path())) {
                    continue;
                }
                const auto rel = std::filesystem::relative(it.path(), root).generic_string();
                auto key = replace_all(strip_extension(rel), '/', '.');
                key = replace_all(std::move(key), '\\', '.');
                auto value = star::schema::parse_schema_file(it.path().string());
                normalize_entry_if_schema_exists(key, &value);
                store_service_->set(key, value);
                ++loaded;
            }
        } else {
            auto key = strip_extension(root.filename().string());
            auto value = star::schema::parse_schema_file(root.string());
            normalize_entry_if_schema_exists(key, &value);
            store_service_->set(key, value);
            ++loaded;
        }

        logI << "ParameterComponent loaded " << loaded << " config(s) from: " << root.string();
    }

    void normalize_entry_if_schema_exists(const std::string& key, star::schema::SchemaValue* value) const {
        if (value == nullptr) {
            return;
        }
        if (key == "planning.motion") {
            const auto typed = star::setting::parameter::planning::MotionParameter::from_value(*value);
            const auto bytes = typed.encode_binary();
            *value = star::setting::parameter::planning::MotionParameter::from_binary(
                std::span<const uint8_t>(bytes.data(), bytes.size())).to_value();
            return;
        }
        const auto schema = context()->schema_registry().find_by_name(key);
        if (not schema) {
            return;
        }
        star::schema::SchemaValidator::validate(*schema, value);
        const auto bytes = star::schema::BinaryCodec::encode(*schema, *value);
        *value = star::schema::BinaryCodec::decode(*schema, std::span<const uint8_t>(bytes.data(), bytes.size()));
    }

    auto decode_set_request_value(const star::setting::service::parameter::set_request::ParameterSetRequest& request) const
        -> star::schema::SchemaValue {
        const auto schema_name = request.schema_name.value_or(request.key);
        if (request.payload_binary.has_value()) {
            const auto& payload_binary = *request.payload_binary;
            if (schema_name == "planning.motion") {
                return star::setting::parameter::planning::MotionParameter::from_binary(
                    std::span<const uint8_t>(
                        reinterpret_cast<const uint8_t*>(payload_binary.data()),
                        payload_binary.size())).to_value();
            }
            const auto schema = context()->schema_registry().find_by_name(schema_name);
            if (not schema) {
                throw star::excep::RuntimeError("ParameterComponent set request unknown schema: ", schema_name);
            }
            return star::schema::BinaryCodec::decode(
                *schema,
                std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(payload_binary.data()),
                    payload_binary.size()));
        }
        if (request.debug_text.has_value()) {
            return star::schema::parse_schema_text(*request.debug_text);
        }
        throw star::excep::RuntimeError("ParameterComponent set request missing payload_binary/debug_text");
    }

    void publish_snapshot() {
        if (not writer_ || not store_service_) {
            return;
        }

        std::map<std::string, star::generated::setting::parameter::snapshot::SnapshotEntry> entries;
        size_t binary_only_entries = 0;
        size_t debug_text_only_entries = 0;
        for (const auto& key : store_service_->list("")) {
            const auto value = store_service_->get(key);
            auto entry = build_snapshot_entry(key, value);
            if (entry.payload_binary.has_value()) {
                ++binary_only_entries;
            } else if (entry.debug_text.has_value()) {
                ++debug_text_only_entries;
            }
            entries.emplace(key, std::move(entry));
        }
        auto snapshot = star::generated::setting::parameter::snapshot::ParameterSnapshot{};
        snapshot.entries = std::move(entries);
        auto payload = snapshot.to_value();
        const auto schema = context()->schema_registry().find_by_name(default_snapshot_schema_type_id().name);
        if (schema) {
            star::schema::SchemaValidator::validate(*schema, &payload);
        }
        auto msg = std::make_shared<const star::generated::setting::parameter::snapshot::ParameterSnapshot>(std::move(snapshot));

        star::transport::MessageView view;
        view.data = star::base::TypedConstPtr::from(msg);
        view.schema_type_id = default_snapshot_schema_type_id();
        view.encode_binary = [msg]() { return msg->encode_binary(); };
        writer_->write(view);
        logI << "ParameterComponent snapshot entries: binary_only=" << binary_only_entries
             << ", debug_text_only=" << debug_text_only_entries;
    }

    auto build_snapshot_entry(const std::string& key, const star::schema::SchemaValue& value) const
        -> star::generated::setting::parameter::snapshot::SnapshotEntry {
        auto entry = star::generated::setting::parameter::snapshot::SnapshotEntry{};
        entry.schema_name = key;
        auto payload_binary = encode_schema_known_entry(key, value);
        if (payload_binary.has_value()) {
            entry.payload_binary = std::move(*payload_binary);
            return entry;
        }
        entry.debug_text = star::schema::dump_schema_text(value);
        return entry;
    }

    auto encode_schema_known_entry(const std::string& key, const star::schema::SchemaValue& value) const
        -> std::optional<std::string> {
        if (key == "planning.motion") {
            const auto typed = star::setting::parameter::planning::MotionParameter::from_value(value);
            const auto bytes = typed.encode_binary();
            return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        }
        const auto schema = context()->schema_registry().find_by_name(key);
        if (not schema) {
            return std::nullopt;
        }
        const auto bytes = star::schema::BinaryCodec::encode(*schema, value);
        return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    std::shared_ptr<ParameterStoreService> store_service_;
    std::shared_ptr<star::transport::Writer> writer_;
    std::shared_ptr<star::transport::ServiceServer> get_server_;
    std::shared_ptr<star::transport::ServiceServer> set_server_;
    std::shared_ptr<star::transport::ServiceServer> list_server_;
};

inline auto register_native_parameter_components(star::runtime::ComponentRegistry& registry) -> void {
    registry.register_component<ParameterComponent>("star::setting::ParameterComponent");
    registry.register_component<ParameterComponent>("star::parameter::Parameter");
}

}  // namespace star::setting

#endif // __PARAMETER_COMPONENT_H__

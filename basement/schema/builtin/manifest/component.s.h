#ifndef __MANIFEST_COMPONENT__
#define __MANIFEST_COMPONENT__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::generated::manifest::component {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "manifest.component";
    out.version = 0;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_GlobalInfo() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "process_group";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "scheduler_policy";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "executor_threads";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ComponentInfo() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "component_library";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "component_class";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "config_file_path";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "static_link";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.default_value = star::schema::SchemaValue(false);
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 7;
        field.name = "component_group";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 8;
        field.name = "priority";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_TriggerInfo() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "cycle_ms";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "task_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "readers";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_QosProfile() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "depth";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "history";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "reliability";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "durability";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_Port() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "channel";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "msg_type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "ack";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "ack_type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "status";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 7;
        field.name = "status_type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 8;
        field.name = "pending_queue_size";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 9;
        field.name = "qos_profile";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "QosProfile";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_QosProfile();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 10;
        field.name = "record";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ComponentConf() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "global";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "GlobalInfo";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_GlobalInfo();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "component";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "ComponentInfo";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_ComponentInfo();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "parameters";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "object";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "trigger";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "TriggerInfo";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_TriggerInfo();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "readers";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Port";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.has_value_kind = true;
        field.value_kind = star::schema::FieldKind::OBJECT;
        field.value_type_name = "Port";
        field.value_children = _build_fields_Port();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "writers";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Port";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.has_value_kind = true;
        field.value_kind = star::schema::FieldKind::OBJECT;
        field.value_type_name = "Port";
        field.value_children = _build_fields_Port();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 7;
        field.name = "commands";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Port";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.has_value_kind = true;
        field.value_kind = star::schema::FieldKind::OBJECT;
        field.value_type_name = "Port";
        field.value_children = _build_fields_Port();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "";
    out.root_name = "ComponentConf";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ComponentConf();
    return out;
}

struct GlobalInfoBuilder;

struct GlobalInfo {
    static constexpr uint32_t kFieldId_process_group = 1;
    static constexpr uint32_t kFieldId_scheduler_policy = 2;
    static constexpr uint32_t kFieldId_executor_threads = 3;

    std::optional<std::string> process_group{std::nullopt};
    std::optional<std::string> scheduler_policy{std::nullopt};
    std::optional<int64_t> executor_threads{std::nullopt};

    auto has_process_group() const -> bool {
        return process_group.has_value();
    }

    auto has_scheduler_policy() const -> bool {
        return scheduler_policy.has_value();
    }

    auto has_executor_threads() const -> bool {
        return executor_threads.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> GlobalInfo {
        auto out = GlobalInfo{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("process_group"); it != obj->end()) {
            out.process_group = schema_read<std::string>(it->second);
        } else {
            out.process_group.reset();
        }
        if (const auto it = obj->find("scheduler_policy"); it != obj->end()) {
            out.scheduler_policy = schema_read<std::string>(it->second);
        } else {
            out.scheduler_policy.reset();
        }
        if (const auto it = obj->find("executor_threads"); it != obj->end()) {
            out.executor_threads = schema_read<int64_t>(it->second);
        } else {
            out.executor_threads.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (process_group.has_value()) {
            obj.emplace("process_group", schema_write(*process_group));
        }
        if (scheduler_policy.has_value()) {
            obj.emplace("scheduler_policy", schema_write(*scheduler_policy));
        }
        if (executor_threads.has_value()) {
            obj.emplace("executor_threads", schema_write(*executor_threads));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> GlobalInfo;
};

struct GlobalInfoView {
    static constexpr size_t kFieldCount = 3;

    GlobalInfoView() = default;
    explicit GlobalInfoView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit GlobalInfoView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_process_group = 0;
    static constexpr size_t kSlotIndex_scheduler_policy = 1;
    static constexpr size_t kSlotIndex_executor_threads = 2;

    auto Has_process_group() const -> bool {
        return not view_.is_null(0);
    }

    auto process_group() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_scheduler_policy() const -> bool {
        return not view_.is_null(1);
    }

    auto scheduler_policy() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_executor_threads() const -> bool {
        return not view_.is_null(2);
    }

    auto executor_threads() const -> int64_t {
        auto v = view_.get_int64(2);
        return v.has_value() ? *v : int64_t{0};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct GlobalInfoBuilder {
    static constexpr size_t kFieldCount = 3;

    GlobalInfoBuilder() : writer_(3) {}

    void set_process_group(std::string_view value) { writer_.set_string(0, value); }
    void set_scheduler_policy(std::string_view value) { writer_.set_string(1, value); }
    void set_executor_threads(int64_t value) { writer_.set_int64(2, value); }

    void clear_process_group() { writer_.set_null(0); }
    void clear_scheduler_policy() { writer_.set_null(1); }
    void clear_executor_threads() { writer_.set_null(2); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto GlobalInfo::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = GlobalInfoBuilder();
    if (process_group.has_value()) {
        builder.set_process_group(*process_group);
    } else {
        builder.clear_process_group();
    }
    if (scheduler_policy.has_value()) {
        builder.set_scheduler_policy(*scheduler_policy);
    } else {
        builder.clear_scheduler_policy();
    }
    if (executor_threads.has_value()) {
        builder.set_executor_threads(*executor_threads);
    } else {
        builder.clear_executor_threads();
    }
    return builder.Serialize();
}

inline auto GlobalInfo::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = GlobalInfoBuilder();
    if (process_group.has_value()) {
        builder.set_process_group(*process_group);
    } else {
        builder.clear_process_group();
    }
    if (scheduler_policy.has_value()) {
        builder.set_scheduler_policy(*scheduler_policy);
    } else {
        builder.clear_scheduler_policy();
    }
    if (executor_threads.has_value()) {
        builder.set_executor_threads(*executor_threads);
    } else {
        builder.clear_executor_threads();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto GlobalInfo::from_fixed_layout(std::span<const uint8_t> data) -> GlobalInfo {
    auto view = GlobalInfoView(data.data(), data.size());
    auto out = GlobalInfo();
    if (view.Has_process_group()) { out.process_group = view.process_group(); }
    if (view.Has_scheduler_policy()) { out.scheduler_policy = view.scheduler_policy(); }
    if (view.Has_executor_threads()) { out.executor_threads = view.executor_threads(); }
    return out;
}

struct ComponentInfoBuilder;

struct ComponentInfo {
    static constexpr uint32_t kFieldId_name = 1;
    static constexpr uint32_t kFieldId_type = 2;
    static constexpr uint32_t kFieldId_component_library = 3;
    static constexpr uint32_t kFieldId_component_class = 4;
    static constexpr uint32_t kFieldId_config_file_path = 5;
    static constexpr uint32_t kFieldId_static_link = 6;
    static constexpr uint32_t kFieldId_component_group = 7;
    static constexpr uint32_t kFieldId_priority = 8;

    std::string name{};
    std::string type{};
    std::optional<std::string> component_library{std::nullopt};
    std::string component_class{};
    std::optional<std::string> config_file_path{std::nullopt};
    bool static_link{false};
    std::optional<std::string> component_group{std::nullopt};
    std::optional<int64_t> priority{std::nullopt};

    auto has_component_library() const -> bool {
        return component_library.has_value();
    }

    auto has_config_file_path() const -> bool {
        return config_file_path.has_value();
    }

    auto has_component_group() const -> bool {
        return component_group.has_value();
    }

    auto has_priority() const -> bool {
        return priority.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ComponentInfo {
        auto out = ComponentInfo{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("name"); it != obj->end()) {
            out.name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: name");
        }
        if (const auto it = obj->find("type"); it != obj->end()) {
            out.type = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: type");
        }
        if (const auto it = obj->find("component_library"); it != obj->end()) {
            out.component_library = schema_read<std::string>(it->second);
        } else {
            out.component_library.reset();
        }
        if (const auto it = obj->find("component_class"); it != obj->end()) {
            out.component_class = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: component_class");
        }
        if (const auto it = obj->find("config_file_path"); it != obj->end()) {
            out.config_file_path = schema_read<std::string>(it->second);
        } else {
            out.config_file_path.reset();
        }
        if (const auto it = obj->find("static_link"); it != obj->end()) {
            out.static_link = schema_read<bool>(it->second);
        } else {
        }
        if (const auto it = obj->find("component_group"); it != obj->end()) {
            out.component_group = schema_read<std::string>(it->second);
        } else {
            out.component_group.reset();
        }
        if (const auto it = obj->find("priority"); it != obj->end()) {
            out.priority = schema_read<int64_t>(it->second);
        } else {
            out.priority.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("name", schema_write(name));
        obj.emplace("type", schema_write(type));
        if (component_library.has_value()) {
            obj.emplace("component_library", schema_write(*component_library));
        }
        obj.emplace("component_class", schema_write(component_class));
        if (config_file_path.has_value()) {
            obj.emplace("config_file_path", schema_write(*config_file_path));
        }
        obj.emplace("static_link", schema_write(static_link));
        if (component_group.has_value()) {
            obj.emplace("component_group", schema_write(*component_group));
        }
        if (priority.has_value()) {
            obj.emplace("priority", schema_write(*priority));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ComponentInfo;
};

struct ComponentInfoView {
    static constexpr size_t kFieldCount = 8;

    ComponentInfoView() = default;
    explicit ComponentInfoView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ComponentInfoView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_name = 0;
    static constexpr size_t kSlotIndex_type = 1;
    static constexpr size_t kSlotIndex_component_library = 2;
    static constexpr size_t kSlotIndex_component_class = 3;
    static constexpr size_t kSlotIndex_config_file_path = 4;
    static constexpr size_t kSlotIndex_static_link = 5;
    static constexpr size_t kSlotIndex_component_group = 6;
    static constexpr size_t kSlotIndex_priority = 7;

    auto Has_name() const -> bool {
        return not view_.is_null(0);
    }

    auto name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_type() const -> bool {
        return not view_.is_null(1);
    }

    auto type() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_component_library() const -> bool {
        return not view_.is_null(2);
    }

    auto component_library() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_component_class() const -> bool {
        return not view_.is_null(3);
    }

    auto component_class() const -> std::string_view {
        auto v = view_.get_string(3);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_config_file_path() const -> bool {
        return not view_.is_null(4);
    }

    auto config_file_path() const -> std::string_view {
        auto v = view_.get_string(4);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_static_link() const -> bool {
        return not view_.is_null(5);
    }

    auto static_link() const -> bool {
        auto v = view_.get_bool(5);
        return v.has_value() ? *v : false;
    }

    auto Has_component_group() const -> bool {
        return not view_.is_null(6);
    }

    auto component_group() const -> std::string_view {
        auto v = view_.get_string(6);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_priority() const -> bool {
        return not view_.is_null(7);
    }

    auto priority() const -> int64_t {
        auto v = view_.get_int64(7);
        return v.has_value() ? *v : int64_t{0};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ComponentInfoBuilder {
    static constexpr size_t kFieldCount = 8;

    ComponentInfoBuilder() : writer_(8) {}

    void set_name(std::string_view value) { writer_.set_string(0, value); }
    void set_type(std::string_view value) { writer_.set_string(1, value); }
    void set_component_library(std::string_view value) { writer_.set_string(2, value); }
    void set_component_class(std::string_view value) { writer_.set_string(3, value); }
    void set_config_file_path(std::string_view value) { writer_.set_string(4, value); }
    void set_static_link(bool value) { writer_.set_bool(5, value); }
    void set_component_group(std::string_view value) { writer_.set_string(6, value); }
    void set_priority(int64_t value) { writer_.set_int64(7, value); }

    void clear_component_library() { writer_.set_null(2); }
    void clear_config_file_path() { writer_.set_null(4); }
    void clear_component_group() { writer_.set_null(6); }
    void clear_priority() { writer_.set_null(7); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto ComponentInfo::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ComponentInfoBuilder();
    builder.set_name(name);
    builder.set_type(type);
    if (component_library.has_value()) {
        builder.set_component_library(*component_library);
    } else {
        builder.clear_component_library();
    }
    builder.set_component_class(component_class);
    if (config_file_path.has_value()) {
        builder.set_config_file_path(*config_file_path);
    } else {
        builder.clear_config_file_path();
    }
    builder.set_static_link(static_link);
    if (component_group.has_value()) {
        builder.set_component_group(*component_group);
    } else {
        builder.clear_component_group();
    }
    if (priority.has_value()) {
        builder.set_priority(*priority);
    } else {
        builder.clear_priority();
    }
    return builder.Serialize();
}

inline auto ComponentInfo::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ComponentInfoBuilder();
    builder.set_name(name);
    builder.set_type(type);
    if (component_library.has_value()) {
        builder.set_component_library(*component_library);
    } else {
        builder.clear_component_library();
    }
    builder.set_component_class(component_class);
    if (config_file_path.has_value()) {
        builder.set_config_file_path(*config_file_path);
    } else {
        builder.clear_config_file_path();
    }
    builder.set_static_link(static_link);
    if (component_group.has_value()) {
        builder.set_component_group(*component_group);
    } else {
        builder.clear_component_group();
    }
    if (priority.has_value()) {
        builder.set_priority(*priority);
    } else {
        builder.clear_priority();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ComponentInfo::from_fixed_layout(std::span<const uint8_t> data) -> ComponentInfo {
    auto view = ComponentInfoView(data.data(), data.size());
    auto out = ComponentInfo();
    out.name = view.name();
    out.type = view.type();
    if (view.Has_component_library()) { out.component_library = view.component_library(); }
    out.component_class = view.component_class();
    if (view.Has_config_file_path()) { out.config_file_path = view.config_file_path(); }
    out.static_link = view.static_link();
    if (view.Has_component_group()) { out.component_group = view.component_group(); }
    if (view.Has_priority()) { out.priority = view.priority(); }
    return out;
}

struct TriggerInfoBuilder;

struct TriggerInfo {
    static constexpr uint32_t kFieldId_type = 1;
    static constexpr uint32_t kFieldId_cycle_ms = 2;
    static constexpr uint32_t kFieldId_task_name = 3;
    static constexpr uint32_t kFieldId_readers = 4;

    std::optional<std::string> type{std::nullopt};
    std::optional<int64_t> cycle_ms{std::nullopt};
    std::optional<std::string> task_name{std::nullopt};
    std::optional<std::vector<std::string>> readers{std::nullopt};

    auto has_type() const -> bool {
        return type.has_value();
    }

    auto has_cycle_ms() const -> bool {
        return cycle_ms.has_value();
    }

    auto has_task_name() const -> bool {
        return task_name.has_value();
    }

    auto has_readers() const -> bool {
        return readers.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> TriggerInfo {
        auto out = TriggerInfo{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("type"); it != obj->end()) {
            out.type = schema_read<std::string>(it->second);
        } else {
            out.type.reset();
        }
        if (const auto it = obj->find("cycle_ms"); it != obj->end()) {
            out.cycle_ms = schema_read<int64_t>(it->second);
        } else {
            out.cycle_ms.reset();
        }
        if (const auto it = obj->find("task_name"); it != obj->end()) {
            out.task_name = schema_read<std::string>(it->second);
        } else {
            out.task_name.reset();
        }
        if (const auto it = obj->find("readers"); it != obj->end()) {
            out.readers = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.readers.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (type.has_value()) {
            obj.emplace("type", schema_write(*type));
        }
        if (cycle_ms.has_value()) {
            obj.emplace("cycle_ms", schema_write(*cycle_ms));
        }
        if (task_name.has_value()) {
            obj.emplace("task_name", schema_write(*task_name));
        }
        if (readers.has_value()) {
            obj.emplace("readers", schema_write(*readers));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> TriggerInfo;
};

struct TriggerInfoView {
    static constexpr size_t kFieldCount = 4;

    TriggerInfoView() = default;
    explicit TriggerInfoView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit TriggerInfoView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_type = 0;
    static constexpr size_t kSlotIndex_cycle_ms = 1;
    static constexpr size_t kSlotIndex_task_name = 2;
    static constexpr size_t kSlotIndex_readers = 3;

    auto Has_type() const -> bool {
        return not view_.is_null(0);
    }

    auto type() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_cycle_ms() const -> bool {
        return not view_.is_null(1);
    }

    auto cycle_ms() const -> int64_t {
        auto v = view_.get_int64(1);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_task_name() const -> bool {
        return not view_.is_null(2);
    }

    auto task_name() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_readers() const -> bool {
        return not view_.is_null(3);
    }

    auto readers() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(3);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct TriggerInfoBuilder {
    static constexpr size_t kFieldCount = 4;

    TriggerInfoBuilder() : writer_(4) {}

    void set_type(std::string_view value) { writer_.set_string(0, value); }
    void set_cycle_ms(int64_t value) { writer_.set_int64(1, value); }
    void set_task_name(std::string_view value) { writer_.set_string(2, value); }
    void set_readers(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(3, bytes); }

    void clear_type() { writer_.set_null(0); }
    void clear_cycle_ms() { writer_.set_null(1); }
    void clear_task_name() { writer_.set_null(2); }
    void clear_readers() { writer_.set_null(3); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto TriggerInfo::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = TriggerInfoBuilder();
    if (type.has_value()) {
        builder.set_type(*type);
    } else {
        builder.clear_type();
    }
    if (cycle_ms.has_value()) {
        builder.set_cycle_ms(*cycle_ms);
    } else {
        builder.clear_cycle_ms();
    }
    if (task_name.has_value()) {
        builder.set_task_name(*task_name);
    } else {
        builder.clear_task_name();
    }
    if (readers.has_value()) {
        builder.set_readers(*readers);
    } else {
        builder.clear_readers();
    }
    return builder.Serialize();
}

inline auto TriggerInfo::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = TriggerInfoBuilder();
    if (type.has_value()) {
        builder.set_type(*type);
    } else {
        builder.clear_type();
    }
    if (cycle_ms.has_value()) {
        builder.set_cycle_ms(*cycle_ms);
    } else {
        builder.clear_cycle_ms();
    }
    if (task_name.has_value()) {
        builder.set_task_name(*task_name);
    } else {
        builder.clear_task_name();
    }
    if (readers.has_value()) {
        builder.set_readers(*readers);
    } else {
        builder.clear_readers();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto TriggerInfo::from_fixed_layout(std::span<const uint8_t> data) -> TriggerInfo {
    auto view = TriggerInfoView(data.data(), data.size());
    auto out = TriggerInfo();
    if (view.Has_type()) { out.type = view.type(); }
    if (view.Has_cycle_ms()) { out.cycle_ms = view.cycle_ms(); }
    if (view.Has_task_name()) { out.task_name = view.task_name(); }
    if (view.Has_readers()) { out.readers = view.readers(); }
    return out;
}

struct QosProfileBuilder;

struct QosProfile {
    static constexpr uint32_t kFieldId_depth = 1;
    static constexpr uint32_t kFieldId_history = 2;
    static constexpr uint32_t kFieldId_reliability = 3;
    static constexpr uint32_t kFieldId_durability = 4;

    std::optional<int64_t> depth{std::nullopt};
    std::optional<std::string> history{std::nullopt};
    std::optional<std::string> reliability{std::nullopt};
    std::optional<std::string> durability{std::nullopt};

    auto has_depth() const -> bool {
        return depth.has_value();
    }

    auto has_history() const -> bool {
        return history.has_value();
    }

    auto has_reliability() const -> bool {
        return reliability.has_value();
    }

    auto has_durability() const -> bool {
        return durability.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> QosProfile {
        auto out = QosProfile{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("depth"); it != obj->end()) {
            out.depth = schema_read<int64_t>(it->second);
        } else {
            out.depth.reset();
        }
        if (const auto it = obj->find("history"); it != obj->end()) {
            out.history = schema_read<std::string>(it->second);
        } else {
            out.history.reset();
        }
        if (const auto it = obj->find("reliability"); it != obj->end()) {
            out.reliability = schema_read<std::string>(it->second);
        } else {
            out.reliability.reset();
        }
        if (const auto it = obj->find("durability"); it != obj->end()) {
            out.durability = schema_read<std::string>(it->second);
        } else {
            out.durability.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (depth.has_value()) {
            obj.emplace("depth", schema_write(*depth));
        }
        if (history.has_value()) {
            obj.emplace("history", schema_write(*history));
        }
        if (reliability.has_value()) {
            obj.emplace("reliability", schema_write(*reliability));
        }
        if (durability.has_value()) {
            obj.emplace("durability", schema_write(*durability));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> QosProfile;
};

struct QosProfileView {
    static constexpr size_t kFieldCount = 4;

    QosProfileView() = default;
    explicit QosProfileView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit QosProfileView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_depth = 0;
    static constexpr size_t kSlotIndex_history = 1;
    static constexpr size_t kSlotIndex_reliability = 2;
    static constexpr size_t kSlotIndex_durability = 3;

    auto Has_depth() const -> bool {
        return not view_.is_null(0);
    }

    auto depth() const -> int64_t {
        auto v = view_.get_int64(0);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_history() const -> bool {
        return not view_.is_null(1);
    }

    auto history() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_reliability() const -> bool {
        return not view_.is_null(2);
    }

    auto reliability() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_durability() const -> bool {
        return not view_.is_null(3);
    }

    auto durability() const -> std::string_view {
        auto v = view_.get_string(3);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct QosProfileBuilder {
    static constexpr size_t kFieldCount = 4;

    QosProfileBuilder() : writer_(4) {}

    void set_depth(int64_t value) { writer_.set_int64(0, value); }
    void set_history(std::string_view value) { writer_.set_string(1, value); }
    void set_reliability(std::string_view value) { writer_.set_string(2, value); }
    void set_durability(std::string_view value) { writer_.set_string(3, value); }

    void clear_depth() { writer_.set_null(0); }
    void clear_history() { writer_.set_null(1); }
    void clear_reliability() { writer_.set_null(2); }
    void clear_durability() { writer_.set_null(3); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto QosProfile::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = QosProfileBuilder();
    if (depth.has_value()) {
        builder.set_depth(*depth);
    } else {
        builder.clear_depth();
    }
    if (history.has_value()) {
        builder.set_history(*history);
    } else {
        builder.clear_history();
    }
    if (reliability.has_value()) {
        builder.set_reliability(*reliability);
    } else {
        builder.clear_reliability();
    }
    if (durability.has_value()) {
        builder.set_durability(*durability);
    } else {
        builder.clear_durability();
    }
    return builder.Serialize();
}

inline auto QosProfile::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = QosProfileBuilder();
    if (depth.has_value()) {
        builder.set_depth(*depth);
    } else {
        builder.clear_depth();
    }
    if (history.has_value()) {
        builder.set_history(*history);
    } else {
        builder.clear_history();
    }
    if (reliability.has_value()) {
        builder.set_reliability(*reliability);
    } else {
        builder.clear_reliability();
    }
    if (durability.has_value()) {
        builder.set_durability(*durability);
    } else {
        builder.clear_durability();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto QosProfile::from_fixed_layout(std::span<const uint8_t> data) -> QosProfile {
    auto view = QosProfileView(data.data(), data.size());
    auto out = QosProfile();
    if (view.Has_depth()) { out.depth = view.depth(); }
    if (view.Has_history()) { out.history = view.history(); }
    if (view.Has_reliability()) { out.reliability = view.reliability(); }
    if (view.Has_durability()) { out.durability = view.durability(); }
    return out;
}

struct PortBuilder;

struct Port {
    static constexpr uint32_t kFieldId_channel = 1;
    static constexpr uint32_t kFieldId_type = 2;
    static constexpr uint32_t kFieldId_msg_type = 3;
    static constexpr uint32_t kFieldId_ack = 4;
    static constexpr uint32_t kFieldId_ack_type = 5;
    static constexpr uint32_t kFieldId_status = 6;
    static constexpr uint32_t kFieldId_status_type = 7;
    static constexpr uint32_t kFieldId_pending_queue_size = 8;
    static constexpr uint32_t kFieldId_qos_profile = 9;
    static constexpr uint32_t kFieldId_record = 10;

    std::string channel{};
    std::optional<std::string> type{std::nullopt};
    std::optional<std::string> msg_type{std::nullopt};
    std::optional<std::string> ack{std::nullopt};
    std::optional<std::string> ack_type{std::nullopt};
    std::optional<std::string> status{std::nullopt};
    std::optional<std::string> status_type{std::nullopt};
    std::optional<int64_t> pending_queue_size{std::nullopt};
    std::optional<QosProfile> qos_profile{std::nullopt};
    std::optional<bool> record{std::nullopt};

    auto has_type() const -> bool {
        return type.has_value();
    }

    auto has_msg_type() const -> bool {
        return msg_type.has_value();
    }

    auto has_ack() const -> bool {
        return ack.has_value();
    }

    auto has_ack_type() const -> bool {
        return ack_type.has_value();
    }

    auto has_status() const -> bool {
        return status.has_value();
    }

    auto has_status_type() const -> bool {
        return status_type.has_value();
    }

    auto has_pending_queue_size() const -> bool {
        return pending_queue_size.has_value();
    }

    auto has_qos_profile() const -> bool {
        return qos_profile.has_value();
    }

    auto has_record() const -> bool {
        return record.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> Port {
        auto out = Port{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("channel"); it != obj->end()) {
            out.channel = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: channel");
        }
        if (const auto it = obj->find("type"); it != obj->end()) {
            out.type = schema_read<std::string>(it->second);
        } else {
            out.type.reset();
        }
        if (const auto it = obj->find("msg_type"); it != obj->end()) {
            out.msg_type = schema_read<std::string>(it->second);
        } else {
            out.msg_type.reset();
        }
        if (const auto it = obj->find("ack"); it != obj->end()) {
            out.ack = schema_read<std::string>(it->second);
        } else {
            out.ack.reset();
        }
        if (const auto it = obj->find("ack_type"); it != obj->end()) {
            out.ack_type = schema_read<std::string>(it->second);
        } else {
            out.ack_type.reset();
        }
        if (const auto it = obj->find("status"); it != obj->end()) {
            out.status = schema_read<std::string>(it->second);
        } else {
            out.status.reset();
        }
        if (const auto it = obj->find("status_type"); it != obj->end()) {
            out.status_type = schema_read<std::string>(it->second);
        } else {
            out.status_type.reset();
        }
        if (const auto it = obj->find("pending_queue_size"); it != obj->end()) {
            out.pending_queue_size = schema_read<int64_t>(it->second);
        } else {
            out.pending_queue_size.reset();
        }
        if (const auto it = obj->find("qos_profile"); it != obj->end()) {
            out.qos_profile = schema_read<QosProfile>(it->second);
        } else {
            out.qos_profile.reset();
        }
        if (const auto it = obj->find("record"); it != obj->end()) {
            out.record = schema_read<bool>(it->second);
        } else {
            out.record.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("channel", schema_write(channel));
        if (type.has_value()) {
            obj.emplace("type", schema_write(*type));
        }
        if (msg_type.has_value()) {
            obj.emplace("msg_type", schema_write(*msg_type));
        }
        if (ack.has_value()) {
            obj.emplace("ack", schema_write(*ack));
        }
        if (ack_type.has_value()) {
            obj.emplace("ack_type", schema_write(*ack_type));
        }
        if (status.has_value()) {
            obj.emplace("status", schema_write(*status));
        }
        if (status_type.has_value()) {
            obj.emplace("status_type", schema_write(*status_type));
        }
        if (pending_queue_size.has_value()) {
            obj.emplace("pending_queue_size", schema_write(*pending_queue_size));
        }
        if (qos_profile.has_value()) {
            obj.emplace("qos_profile", schema_write(*qos_profile));
        }
        if (record.has_value()) {
            obj.emplace("record", schema_write(*record));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Port;
};

struct PortView {
    static constexpr size_t kFieldCount = 10;

    PortView() = default;
    explicit PortView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit PortView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_channel = 0;
    static constexpr size_t kSlotIndex_type = 1;
    static constexpr size_t kSlotIndex_msg_type = 2;
    static constexpr size_t kSlotIndex_ack = 3;
    static constexpr size_t kSlotIndex_ack_type = 4;
    static constexpr size_t kSlotIndex_status = 5;
    static constexpr size_t kSlotIndex_status_type = 6;
    static constexpr size_t kSlotIndex_pending_queue_size = 7;
    static constexpr size_t kSlotIndex_qos_profile = 8;
    static constexpr size_t kSlotIndex_record = 9;

    auto Has_channel() const -> bool {
        return not view_.is_null(0);
    }

    auto channel() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_type() const -> bool {
        return not view_.is_null(1);
    }

    auto type() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_msg_type() const -> bool {
        return not view_.is_null(2);
    }

    auto msg_type() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_ack() const -> bool {
        return not view_.is_null(3);
    }

    auto ack() const -> std::string_view {
        auto v = view_.get_string(3);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_ack_type() const -> bool {
        return not view_.is_null(4);
    }

    auto ack_type() const -> std::string_view {
        auto v = view_.get_string(4);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_status() const -> bool {
        return not view_.is_null(5);
    }

    auto status() const -> std::string_view {
        auto v = view_.get_string(5);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_status_type() const -> bool {
        return not view_.is_null(6);
    }

    auto status_type() const -> std::string_view {
        auto v = view_.get_string(6);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_pending_queue_size() const -> bool {
        return not view_.is_null(7);
    }

    auto pending_queue_size() const -> int64_t {
        auto v = view_.get_int64(7);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_qos_profile() const -> bool {
        return not view_.is_null(8);
    }

    auto qos_profile_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(8);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_record() const -> bool {
        return not view_.is_null(9);
    }

    auto record() const -> bool {
        auto v = view_.get_bool(9);
        return v.has_value() ? *v : false;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct PortBuilder {
    static constexpr size_t kFieldCount = 10;

    PortBuilder() : writer_(10) {}

    void set_channel(std::string_view value) { writer_.set_string(0, value); }
    void set_type(std::string_view value) { writer_.set_string(1, value); }
    void set_msg_type(std::string_view value) { writer_.set_string(2, value); }
    void set_ack(std::string_view value) { writer_.set_string(3, value); }
    void set_ack_type(std::string_view value) { writer_.set_string(4, value); }
    void set_status(std::string_view value) { writer_.set_string(5, value); }
    void set_status_type(std::string_view value) { writer_.set_string(6, value); }
    void set_pending_queue_size(int64_t value) { writer_.set_int64(7, value); }
    void set_qos_profile_bytes(std::span<const uint8_t> value) { writer_.set_object(8, value); }
    void set_record(bool value) { writer_.set_bool(9, value); }

    void clear_type() { writer_.set_null(1); }
    void clear_msg_type() { writer_.set_null(2); }
    void clear_ack() { writer_.set_null(3); }
    void clear_ack_type() { writer_.set_null(4); }
    void clear_status() { writer_.set_null(5); }
    void clear_status_type() { writer_.set_null(6); }
    void clear_pending_queue_size() { writer_.set_null(7); }
    void clear_qos_profile() { writer_.set_null(8); }
    void clear_record() { writer_.set_null(9); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto Port::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = PortBuilder();
    builder.set_channel(channel);
    if (type.has_value()) {
        builder.set_type(*type);
    } else {
        builder.clear_type();
    }
    if (msg_type.has_value()) {
        builder.set_msg_type(*msg_type);
    } else {
        builder.clear_msg_type();
    }
    if (ack.has_value()) {
        builder.set_ack(*ack);
    } else {
        builder.clear_ack();
    }
    if (ack_type.has_value()) {
        builder.set_ack_type(*ack_type);
    } else {
        builder.clear_ack_type();
    }
    if (status.has_value()) {
        builder.set_status(*status);
    } else {
        builder.clear_status();
    }
    if (status_type.has_value()) {
        builder.set_status_type(*status_type);
    } else {
        builder.clear_status_type();
    }
    if (pending_queue_size.has_value()) {
        builder.set_pending_queue_size(*pending_queue_size);
    } else {
        builder.clear_pending_queue_size();
    }
    if (qos_profile.has_value()) {
        { auto bytes = qos_profile->encode_fixed_layout(); builder.set_qos_profile_bytes(bytes); }
    } else {
        builder.clear_qos_profile();
    }
    if (record.has_value()) {
        builder.set_record(*record);
    } else {
        builder.clear_record();
    }
    return builder.Serialize();
}

inline auto Port::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = PortBuilder();
    builder.set_channel(channel);
    if (type.has_value()) {
        builder.set_type(*type);
    } else {
        builder.clear_type();
    }
    if (msg_type.has_value()) {
        builder.set_msg_type(*msg_type);
    } else {
        builder.clear_msg_type();
    }
    if (ack.has_value()) {
        builder.set_ack(*ack);
    } else {
        builder.clear_ack();
    }
    if (ack_type.has_value()) {
        builder.set_ack_type(*ack_type);
    } else {
        builder.clear_ack_type();
    }
    if (status.has_value()) {
        builder.set_status(*status);
    } else {
        builder.clear_status();
    }
    if (status_type.has_value()) {
        builder.set_status_type(*status_type);
    } else {
        builder.clear_status_type();
    }
    if (pending_queue_size.has_value()) {
        builder.set_pending_queue_size(*pending_queue_size);
    } else {
        builder.clear_pending_queue_size();
    }
    if (qos_profile.has_value()) {
        { auto bytes = qos_profile->encode_fixed_layout(); builder.set_qos_profile_bytes(bytes); }
    } else {
        builder.clear_qos_profile();
    }
    if (record.has_value()) {
        builder.set_record(*record);
    } else {
        builder.clear_record();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto Port::from_fixed_layout(std::span<const uint8_t> data) -> Port {
    auto view = PortView(data.data(), data.size());
    auto out = Port();
    out.channel = view.channel();
    if (view.Has_type()) { out.type = view.type(); }
    if (view.Has_msg_type()) { out.msg_type = view.msg_type(); }
    if (view.Has_ack()) { out.ack = view.ack(); }
    if (view.Has_ack_type()) { out.ack_type = view.ack_type(); }
    if (view.Has_status()) { out.status = view.status(); }
    if (view.Has_status_type()) { out.status_type = view.status_type(); }
    if (view.Has_pending_queue_size()) { out.pending_queue_size = view.pending_queue_size(); }
    if (view.Has_qos_profile()) { auto bytes = view.qos_profile_bytes(); out.qos_profile = QosProfile::from_fixed_layout(bytes); }
    if (view.Has_record()) { out.record = view.record(); }
    return out;
}

struct ComponentConfBuilder;

struct ComponentConf {
    static constexpr uint32_t kFieldId_global = 1;
    static constexpr uint32_t kFieldId_component = 2;
    static constexpr uint32_t kFieldId_parameters = 3;
    static constexpr uint32_t kFieldId_trigger = 4;
    static constexpr uint32_t kFieldId_readers = 5;
    static constexpr uint32_t kFieldId_writers = 6;
    static constexpr uint32_t kFieldId_commands = 7;

    std::optional<GlobalInfo> global{std::nullopt};
    ComponentInfo component{};
    std::optional<star::schema::SchemaObject> parameters{std::nullopt};
    std::optional<TriggerInfo> trigger{std::nullopt};
    std::optional<std::map<std::string, Port>> readers{std::nullopt};
    std::optional<std::map<std::string, Port>> writers{std::nullopt};
    std::optional<std::map<std::string, Port>> commands{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_global() const -> bool {
        return global.has_value();
    }

    auto has_parameters() const -> bool {
        return parameters.has_value();
    }

    auto has_trigger() const -> bool {
        return trigger.has_value();
    }

    auto has_readers() const -> bool {
        return readers.has_value();
    }

    auto has_writers() const -> bool {
        return writers.has_value();
    }

    auto has_commands() const -> bool {
        return commands.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ComponentConf {
        auto out = ComponentConf{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("global"); it != obj->end()) {
            out.global = schema_read<GlobalInfo>(it->second);
        } else {
            out.global.reset();
        }
        if (const auto it = obj->find("component"); it != obj->end()) {
            out.component = schema_read<ComponentInfo>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: component");
        }
        if (const auto it = obj->find("parameters"); it != obj->end()) {
            out.parameters = schema_read<star::schema::SchemaObject>(it->second);
        } else {
            out.parameters.reset();
        }
        if (const auto it = obj->find("trigger"); it != obj->end()) {
            out.trigger = schema_read<TriggerInfo>(it->second);
        } else {
            out.trigger.reset();
        }
        if (const auto it = obj->find("readers"); it != obj->end()) {
            out.readers = schema_read<std::map<std::string, Port>>(it->second);
        } else {
            out.readers.reset();
        }
        if (const auto it = obj->find("writers"); it != obj->end()) {
            out.writers = schema_read<std::map<std::string, Port>>(it->second);
        } else {
            out.writers.reset();
        }
        if (const auto it = obj->find("commands"); it != obj->end()) {
            out.commands = schema_read<std::map<std::string, Port>>(it->second);
        } else {
            out.commands.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (global.has_value()) {
            obj.emplace("global", schema_write(*global));
        }
        obj.emplace("component", schema_write(component));
        if (parameters.has_value()) {
            obj.emplace("parameters", schema_write(*parameters));
        }
        if (trigger.has_value()) {
            obj.emplace("trigger", schema_write(*trigger));
        }
        if (readers.has_value()) {
            obj.emplace("readers", schema_write(*readers));
        }
        if (writers.has_value()) {
            obj.emplace("writers", schema_write(*writers));
        }
        if (commands.has_value()) {
            obj.emplace("commands", schema_write(*commands));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    static auto schema_type_id() -> star::schema::SchemaTypeId {
        return _schema_type_id();
    }

    static auto schema_descriptor() -> star::schema::SchemaDescriptor {
        return _schema_descriptor();
    }

    static void register_schema(star::schema::SchemaRegistry& registry) {
        registry.register_schema(schema_descriptor());
    }

    auto encode_binary() const -> std::vector<uint8_t> {
        return star::schema::BinaryCodec::encode(_schema_descriptor(), to_value(), unknown_fields_);
    }

    static auto from_binary(std::span<const uint8_t> bytes) -> ComponentConf {
        auto decoded = star::schema::BinaryCodec::decode_with_unknown(_schema_descriptor(), bytes);
        auto out = from_value(decoded.value);
        out.unknown_fields_ = std::move(decoded.unknown_fields);
        return out;
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return unknown_fields_;
    }

    void set_unknown_fields(star::schema::UnknownFieldSet unknown_fields) {
        unknown_fields_ = std::move(unknown_fields);
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ComponentConf;
};

struct ComponentConfView {
    static constexpr size_t kFieldCount = 7;

    ComponentConfView() = default;
    explicit ComponentConfView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ComponentConfView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ComponentConfView {
        return ComponentConfView(bytes);
    }

    static constexpr size_t kSlotIndex_global = 0;
    static constexpr size_t kSlotIndex_component = 1;
    static constexpr size_t kSlotIndex_parameters = 2;
    static constexpr size_t kSlotIndex_trigger = 3;
    static constexpr size_t kSlotIndex_readers = 4;
    static constexpr size_t kSlotIndex_writers = 5;
    static constexpr size_t kSlotIndex_commands = 6;

    auto Has_global() const -> bool {
        return not view_.is_null(0);
    }

    auto global_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_component() const -> bool {
        return not view_.is_null(1);
    }

    auto component_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(1);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_parameters() const -> bool {
        return not view_.is_null(2);
    }

    auto parameters() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_trigger() const -> bool {
        return not view_.is_null(3);
    }

    auto trigger_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(3);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_readers() const -> bool {
        return not view_.is_null(4);
    }

    auto readers_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_map_bytes(4);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_writers() const -> bool {
        return not view_.is_null(5);
    }

    auto writers_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_map_bytes(5);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_commands() const -> bool {
        return not view_.is_null(6);
    }

    auto commands_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_map_bytes(6);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ComponentConfBuilder {
    static constexpr size_t kFieldCount = 7;

    ComponentConfBuilder() : writer_(7) {}

    void set_global_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }
    void set_component_bytes(std::span<const uint8_t> value) { writer_.set_object(1, value); }
    void set_parameters(std::string_view value) { writer_.set_string(2, value); }
    void set_trigger_bytes(std::span<const uint8_t> value) { writer_.set_object(3, value); }
    void set_readers_bytes(std::span<const uint8_t> value) { writer_.set_map(4, value); }
    void set_writers_bytes(std::span<const uint8_t> value) { writer_.set_map(5, value); }
    void set_commands_bytes(std::span<const uint8_t> value) { writer_.set_map(6, value); }

    void clear_global() { writer_.set_null(0); }
    void clear_parameters() { writer_.set_null(2); }
    void clear_trigger() { writer_.set_null(3); }
    void clear_readers() { writer_.set_null(4); }
    void clear_writers() { writer_.set_null(5); }
    void clear_commands() { writer_.set_null(6); }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        return writer_.serialize_into(buf, cap);
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }

    auto Serialize() const -> std::vector<uint8_t> {
        return writer_.serialize();
    }

  private:
    star::transport::FixedLayoutWriter writer_;
};

inline auto ComponentConf::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ComponentConfBuilder();
    if (global.has_value()) {
        { auto bytes = global->encode_fixed_layout(); builder.set_global_bytes(bytes); }
    } else {
        builder.clear_global();
    }
    { auto bytes = component.encode_fixed_layout(); builder.set_component_bytes(bytes); }
    if (parameters.has_value()) {
        { auto text = star::schema::dump_schema_text(star::schema::SchemaValue(*parameters)); builder.set_parameters(text); }
    } else {
        builder.clear_parameters();
    }
    if (trigger.has_value()) {
        { auto bytes = trigger->encode_fixed_layout(); builder.set_trigger_bytes(bytes); }
    } else {
        builder.clear_trigger();
    }
    if (readers.has_value()) {
        { auto bytes = star::transport::encode_object_map(*readers); builder.set_readers_bytes(bytes); }
    } else {
        builder.clear_readers();
    }
    if (writers.has_value()) {
        { auto bytes = star::transport::encode_object_map(*writers); builder.set_writers_bytes(bytes); }
    } else {
        builder.clear_writers();
    }
    if (commands.has_value()) {
        { auto bytes = star::transport::encode_object_map(*commands); builder.set_commands_bytes(bytes); }
    } else {
        builder.clear_commands();
    }
    return builder.Serialize();
}

inline auto ComponentConf::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ComponentConfBuilder();
    if (global.has_value()) {
        { auto bytes = global->encode_fixed_layout(); builder.set_global_bytes(bytes); }
    } else {
        builder.clear_global();
    }
    { auto bytes = component.encode_fixed_layout(); builder.set_component_bytes(bytes); }
    if (parameters.has_value()) {
        { auto text = star::schema::dump_schema_text(star::schema::SchemaValue(*parameters)); builder.set_parameters(text); }
    } else {
        builder.clear_parameters();
    }
    if (trigger.has_value()) {
        { auto bytes = trigger->encode_fixed_layout(); builder.set_trigger_bytes(bytes); }
    } else {
        builder.clear_trigger();
    }
    if (readers.has_value()) {
        { auto bytes = star::transport::encode_object_map(*readers); builder.set_readers_bytes(bytes); }
    } else {
        builder.clear_readers();
    }
    if (writers.has_value()) {
        { auto bytes = star::transport::encode_object_map(*writers); builder.set_writers_bytes(bytes); }
    } else {
        builder.clear_writers();
    }
    if (commands.has_value()) {
        { auto bytes = star::transport::encode_object_map(*commands); builder.set_commands_bytes(bytes); }
    } else {
        builder.clear_commands();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ComponentConf::from_fixed_layout(std::span<const uint8_t> data) -> ComponentConf {
    auto view = ComponentConfView(data.data(), data.size());
    auto out = ComponentConf();
    if (view.Has_global()) { auto bytes = view.global_bytes(); out.global = GlobalInfo::from_fixed_layout(bytes); }
    { auto bytes = view.component_bytes(); out.component = ComponentInfo::from_fixed_layout(bytes); }
    if (view.Has_parameters()) { auto text = view.parameters(); auto val = star::schema::parse_schema_text(text); out.parameters = val.as_object(); }
    if (view.Has_trigger()) { auto bytes = view.trigger_bytes(); out.trigger = TriggerInfo::from_fixed_layout(bytes); }
    if (view.Has_readers()) { auto bytes = view.readers_bytes(); out.readers = star::transport::decode_object_map<Port>(bytes); }
    if (view.Has_writers()) { auto bytes = view.writers_bytes(); out.writers = star::transport::decode_object_map<Port>(bytes); }
    if (view.Has_commands()) { auto bytes = view.commands_bytes(); out.commands = star::transport::decode_object_map<Port>(bytes); }
    return out;
}

using ComponentConfSchema = ComponentConf;

}  // namespace star::generated::manifest::component

#endif // __MANIFEST_COMPONENT__

#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_MANIFEST_COMPONENT_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_MANIFEST_COMPONENT_S_H__

#include "basement/schema/binary_codec.h"
#include "basement/schema/schema_ast.h"
#include "basement/schema/schema_registry.h"
#include "libraries/common/schema/schema_value.h"

#include <concepts>
#include <cstring>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace star::generated::manifest::component {

template <typename _ValueType>
concept SchemaStructType = requires(const star::schema::SchemaValue& value, const _ValueType& item) {
    { _ValueType::from_value(value) } -> std::same_as<_ValueType>;
    { item.to_value() } -> std::same_as<star::schema::SchemaValue>;
};

template <typename _ValueType>
struct IsStdVector : std::false_type {
};

template <typename _ItemType, typename _Allocator>
struct IsStdVector<std::vector<_ItemType, _Allocator>> : std::true_type {
    using Item = _ItemType;
};

template <typename _ValueType>
struct IsStdMap : std::false_type {
};

template <typename _MappedType, typename _Compare, typename _Allocator>
struct IsStdMap<std::map<std::string, _MappedType, _Compare, _Allocator>> : std::true_type {
    using Item = _MappedType;
};

template <typename _ValueType>
auto schema_read(const star::schema::SchemaValue& value) -> _ValueType {
    if constexpr (IsStdVector<_ValueType>::value) {
        using _Item = typename IsStdVector<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& arr = value.as_array();
        out.reserve(arr.size());
        for (const auto& item : arr) {
            out.emplace_back(schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (IsStdMap<_ValueType>::value) {
        using _Item = typename IsStdMap<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& obj = value.as_object();
        for (const auto& [key, item] : obj) {
            out.emplace(key, schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (std::is_same_v<_ValueType, std::string>) {
        return value.as<std::string>();
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, bool>) {
        return value.as<bool>();
    } else if constexpr (std::is_same_v<_ValueType, int64_t>) {
        return value.as<int64_t>();
    } else if constexpr (std::is_same_v<_ValueType, double>) {
        return value.as<double>();
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaObject>) {
        return value.as_object();
    } else if constexpr (SchemaStructType<_ValueType>) {
        return _ValueType::from_value(value);
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_read type");
    }
}

template <typename _ValueType>
auto schema_write(const _ValueType& value) -> star::schema::SchemaValue {
    if constexpr (IsStdVector<_ValueType>::value) {
        auto out = star::schema::SchemaArray{};
        out.reserve(value.size());
        for (const auto& item : value) {
            out.emplace_back(schema_write(item));
        }
        return star::schema::SchemaValue(std::move(out));
    } else if constexpr (IsStdMap<_ValueType>::value) {
        auto out = star::schema::SchemaObject{};
        for (const auto& [key, item] : value) {
            out.emplace(key, schema_write(item));
        }
        return star::schema::SchemaValue(std::move(out));
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, std::string> || std::is_same_v<_ValueType, bool> ||
                  std::is_same_v<_ValueType, int64_t> || std::is_same_v<_ValueType, double>) {
        return star::schema::schema_value_from(value);
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaObject>) {
        return star::schema::SchemaValue(value);
    } else if constexpr (SchemaStructType<_ValueType>) {
        return value.to_value();
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_write type");
    }
}

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
};

struct GlobalInfoView {
    GlobalInfo value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> GlobalInfoView {
        return GlobalInfoView{GlobalInfo::from_value(value)};
    }

    auto Has_process_group() const -> bool {
        return value.has_process_group();
    }

    auto process_group() const -> const std::string& {
        if (not value.process_group.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: process_group");
        }
        return *value.process_group;
    }

    auto Has_scheduler_policy() const -> bool {
        return value.has_scheduler_policy();
    }

    auto scheduler_policy() const -> const std::string& {
        if (not value.scheduler_policy.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: scheduler_policy");
        }
        return *value.scheduler_policy;
    }

    auto Has_executor_threads() const -> bool {
        return value.has_executor_threads();
    }

    auto executor_threads() const -> const int64_t& {
        if (not value.executor_threads.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: executor_threads");
        }
        return *value.executor_threads;
    }

};

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
};

struct ComponentInfoView {
    ComponentInfo value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ComponentInfoView {
        return ComponentInfoView{ComponentInfo::from_value(value)};
    }

    auto Has_name() const -> bool {
        return true;
    }

    auto name() const -> const std::string& {
        return value.name;
    }

    auto Has_type() const -> bool {
        return true;
    }

    auto type() const -> const std::string& {
        return value.type;
    }

    auto Has_component_library() const -> bool {
        return value.has_component_library();
    }

    auto component_library() const -> const std::string& {
        if (not value.component_library.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: component_library");
        }
        return *value.component_library;
    }

    auto Has_component_class() const -> bool {
        return true;
    }

    auto component_class() const -> const std::string& {
        return value.component_class;
    }

    auto Has_config_file_path() const -> bool {
        return value.has_config_file_path();
    }

    auto config_file_path() const -> const std::string& {
        if (not value.config_file_path.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: config_file_path");
        }
        return *value.config_file_path;
    }

    auto Has_static_link() const -> bool {
        return true;
    }

    auto static_link() const -> const bool& {
        return value.static_link;
    }

    auto Has_component_group() const -> bool {
        return value.has_component_group();
    }

    auto component_group() const -> const std::string& {
        if (not value.component_group.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: component_group");
        }
        return *value.component_group;
    }

    auto Has_priority() const -> bool {
        return value.has_priority();
    }

    auto priority() const -> const int64_t& {
        if (not value.priority.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: priority");
        }
        return *value.priority;
    }

};

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
};

struct TriggerInfoView {
    TriggerInfo value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> TriggerInfoView {
        return TriggerInfoView{TriggerInfo::from_value(value)};
    }

    auto Has_type() const -> bool {
        return value.has_type();
    }

    auto type() const -> const std::string& {
        if (not value.type.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: type");
        }
        return *value.type;
    }

    auto Has_cycle_ms() const -> bool {
        return value.has_cycle_ms();
    }

    auto cycle_ms() const -> const int64_t& {
        if (not value.cycle_ms.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: cycle_ms");
        }
        return *value.cycle_ms;
    }

    auto Has_task_name() const -> bool {
        return value.has_task_name();
    }

    auto task_name() const -> const std::string& {
        if (not value.task_name.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: task_name");
        }
        return *value.task_name;
    }

    auto Has_readers() const -> bool {
        return value.has_readers();
    }

    auto readers() const -> const std::vector<std::string>& {
        if (not value.readers.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: readers");
        }
        return *value.readers;
    }

};

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
};

struct QosProfileView {
    QosProfile value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> QosProfileView {
        return QosProfileView{QosProfile::from_value(value)};
    }

    auto Has_depth() const -> bool {
        return value.has_depth();
    }

    auto depth() const -> const int64_t& {
        if (not value.depth.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: depth");
        }
        return *value.depth;
    }

    auto Has_history() const -> bool {
        return value.has_history();
    }

    auto history() const -> const std::string& {
        if (not value.history.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: history");
        }
        return *value.history;
    }

    auto Has_reliability() const -> bool {
        return value.has_reliability();
    }

    auto reliability() const -> const std::string& {
        if (not value.reliability.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: reliability");
        }
        return *value.reliability;
    }

    auto Has_durability() const -> bool {
        return value.has_durability();
    }

    auto durability() const -> const std::string& {
        if (not value.durability.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: durability");
        }
        return *value.durability;
    }

};

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
};

struct PortView {
    Port value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> PortView {
        return PortView{Port::from_value(value)};
    }

    auto Has_channel() const -> bool {
        return true;
    }

    auto channel() const -> const std::string& {
        return value.channel;
    }

    auto Has_type() const -> bool {
        return value.has_type();
    }

    auto type() const -> const std::string& {
        if (not value.type.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: type");
        }
        return *value.type;
    }

    auto Has_msg_type() const -> bool {
        return value.has_msg_type();
    }

    auto msg_type() const -> const std::string& {
        if (not value.msg_type.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: msg_type");
        }
        return *value.msg_type;
    }

    auto Has_ack() const -> bool {
        return value.has_ack();
    }

    auto ack() const -> const std::string& {
        if (not value.ack.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: ack");
        }
        return *value.ack;
    }

    auto Has_ack_type() const -> bool {
        return value.has_ack_type();
    }

    auto ack_type() const -> const std::string& {
        if (not value.ack_type.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: ack_type");
        }
        return *value.ack_type;
    }

    auto Has_status() const -> bool {
        return value.has_status();
    }

    auto status() const -> const std::string& {
        if (not value.status.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: status");
        }
        return *value.status;
    }

    auto Has_status_type() const -> bool {
        return value.has_status_type();
    }

    auto status_type() const -> const std::string& {
        if (not value.status_type.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: status_type");
        }
        return *value.status_type;
    }

    auto Has_pending_queue_size() const -> bool {
        return value.has_pending_queue_size();
    }

    auto pending_queue_size() const -> const int64_t& {
        if (not value.pending_queue_size.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: pending_queue_size");
        }
        return *value.pending_queue_size;
    }

    auto Has_qos_profile() const -> bool {
        return value.has_qos_profile();
    }

    auto qos_profile() const -> const QosProfile& {
        if (not value.qos_profile.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: qos_profile");
        }
        return *value.qos_profile;
    }

    auto Has_record() const -> bool {
        return value.has_record();
    }

    auto record() const -> const bool& {
        if (not value.record.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: record");
        }
        return *value.record;
    }

};

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
};

struct ComponentConfView {
    ComponentConf value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ComponentConfView {
        return ComponentConfView{ComponentConf::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ComponentConfView {
        return ComponentConfView{ComponentConf::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_global() const -> bool {
        return value.has_global();
    }

    auto global() const -> const GlobalInfo& {
        if (not value.global.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: global");
        }
        return *value.global;
    }

    auto Has_component() const -> bool {
        return true;
    }

    auto component() const -> const ComponentInfo& {
        return value.component;
    }

    auto Has_parameters() const -> bool {
        return value.has_parameters();
    }

    auto parameters() const -> const star::schema::SchemaObject& {
        if (not value.parameters.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: parameters");
        }
        return *value.parameters;
    }

    auto Has_trigger() const -> bool {
        return value.has_trigger();
    }

    auto trigger() const -> const TriggerInfo& {
        if (not value.trigger.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: trigger");
        }
        return *value.trigger;
    }

    auto Has_readers() const -> bool {
        return value.has_readers();
    }

    auto readers() const -> const std::map<std::string, Port>& {
        if (not value.readers.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: readers");
        }
        return *value.readers;
    }

    auto Has_writers() const -> bool {
        return value.has_writers();
    }

    auto writers() const -> const std::map<std::string, Port>& {
        if (not value.writers.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: writers");
        }
        return *value.writers;
    }

    auto Has_commands() const -> bool {
        return value.has_commands();
    }

    auto commands() const -> const std::map<std::string, Port>& {
        if (not value.commands.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: commands");
        }
        return *value.commands;
    }

};

struct ComponentConfBuilder {
    ComponentConf value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const ComponentConf& value) -> ComponentConfBuilder {
        return ComponentConfBuilder{value, value.unknown_fields()};
    }

    void set_unknown_fields(star::schema::UnknownFieldSet value_in) {
        unknown_fields = std::move(value_in);
    }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        if (buf == nullptr) {
            return 0;
        }
        auto tmp = value;
        tmp.set_unknown_fields(unknown_fields);
        const auto bytes = tmp.encode_binary();
        if (bytes.size() > cap) {
            return 0;
        }
        if (not bytes.empty()) {
            std::memcpy(buf, bytes.data(), bytes.size());
        }
        return bytes.size();
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }
};

using ComponentConfSchema = ComponentConf;

}  // namespace star::generated::manifest::component

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_MANIFEST_COMPONENT_S_H__

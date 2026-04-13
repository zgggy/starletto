#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_RUNTIME_COMPONENT_LOAD_PLAN_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_RUNTIME_COMPONENT_LOAD_PLAN_S_H__

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

namespace star::generated::runtime::component_load_plan {

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
    out.name = "runtime.component_load_plan";
    out.version = 0;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_TriggerPlan() -> std::vector<star::schema::FieldDescriptor> {
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
        field.name = "task_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "cycle_ms";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
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

inline auto _build_fields_PortPlan() -> std::vector<star::schema::FieldDescriptor> {
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
        field.name = "channel";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "type_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "record";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "direction";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "qos_depth";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 7;
        field.name = "qos_drop_policy";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ServicePortPlan() -> std::vector<star::schema::FieldDescriptor> {
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
        field.name = "channel";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "request_type_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "response_type_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "role";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ComponentLoadPlan() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "module_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "component_name";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "component_group";
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
        field.name = "component_library";
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
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 7;
        field.name = "priority";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 8;
        field.name = "component_conf_path";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 9;
        field.name = "module_conf_path";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 10;
        field.name = "module_dir";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 11;
        field.name = "module_config_dir";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 12;
        field.name = "config_file_path";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 13;
        field.name = "process_group";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 14;
        field.name = "scheduler_type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 15;
        field.name = "executor_threads";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 16;
        field.name = "trigger";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "TriggerPlan";
        field.optional = true;
        field.repeated = false;
        field.children = _build_fields_TriggerPlan();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 17;
        field.name = "inputs";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "PortPlan";
        field.optional = true;
        field.repeated = true;
        field.children = _build_fields_PortPlan();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 18;
        field.name = "outputs";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "PortPlan";
        field.optional = true;
        field.repeated = true;
        field.children = _build_fields_PortPlan();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 19;
        field.name = "services";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "ServicePortPlan";
        field.optional = true;
        field.repeated = true;
        field.children = _build_fields_ServicePortPlan();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "";
    out.root_name = "ComponentLoadPlan";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ComponentLoadPlan();
    return out;
}

struct TriggerPlan {
    static constexpr uint32_t kFieldId_type = 1;
    static constexpr uint32_t kFieldId_task_name = 2;
    static constexpr uint32_t kFieldId_cycle_ms = 3;
    static constexpr uint32_t kFieldId_readers = 4;

    std::optional<std::string> type{std::nullopt};
    std::optional<std::string> task_name{std::nullopt};
    std::optional<int64_t> cycle_ms{std::nullopt};
    std::optional<std::vector<std::string>> readers{std::nullopt};

    auto has_type() const -> bool {
        return type.has_value();
    }

    auto has_task_name() const -> bool {
        return task_name.has_value();
    }

    auto has_cycle_ms() const -> bool {
        return cycle_ms.has_value();
    }

    auto has_readers() const -> bool {
        return readers.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> TriggerPlan {
        auto out = TriggerPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("type"); it != obj->end()) {
            out.type = schema_read<std::string>(it->second);
        } else {
            out.type.reset();
        }
        if (const auto it = obj->find("task_name"); it != obj->end()) {
            out.task_name = schema_read<std::string>(it->second);
        } else {
            out.task_name.reset();
        }
        if (const auto it = obj->find("cycle_ms"); it != obj->end()) {
            out.cycle_ms = schema_read<int64_t>(it->second);
        } else {
            out.cycle_ms.reset();
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
        if (task_name.has_value()) {
            obj.emplace("task_name", schema_write(*task_name));
        }
        if (cycle_ms.has_value()) {
            obj.emplace("cycle_ms", schema_write(*cycle_ms));
        }
        if (readers.has_value()) {
            obj.emplace("readers", schema_write(*readers));
        }
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct TriggerPlanView {
    TriggerPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> TriggerPlanView {
        return TriggerPlanView{TriggerPlan::from_value(value)};
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

    auto Has_task_name() const -> bool {
        return value.has_task_name();
    }

    auto task_name() const -> const std::string& {
        if (not value.task_name.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: task_name");
        }
        return *value.task_name;
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

struct PortPlan {
    static constexpr uint32_t kFieldId_name = 1;
    static constexpr uint32_t kFieldId_channel = 2;
    static constexpr uint32_t kFieldId_type_name = 3;
    static constexpr uint32_t kFieldId_record = 4;
    static constexpr uint32_t kFieldId_direction = 5;
    static constexpr uint32_t kFieldId_qos_depth = 6;
    static constexpr uint32_t kFieldId_qos_drop_policy = 7;

    std::string name{};
    std::optional<std::string> channel{std::nullopt};
    std::optional<std::string> type_name{std::nullopt};
    std::optional<bool> record{std::nullopt};
    std::optional<std::string> direction{std::nullopt};
    std::optional<int64_t> qos_depth{std::nullopt};
    std::optional<std::string> qos_drop_policy{std::nullopt};

    auto has_channel() const -> bool {
        return channel.has_value();
    }

    auto has_type_name() const -> bool {
        return type_name.has_value();
    }

    auto has_record() const -> bool {
        return record.has_value();
    }

    auto has_direction() const -> bool {
        return direction.has_value();
    }

    auto has_qos_depth() const -> bool {
        return qos_depth.has_value();
    }

    auto has_qos_drop_policy() const -> bool {
        return qos_drop_policy.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> PortPlan {
        auto out = PortPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("name"); it != obj->end()) {
            out.name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: name");
        }
        if (const auto it = obj->find("channel"); it != obj->end()) {
            out.channel = schema_read<std::string>(it->second);
        } else {
            out.channel.reset();
        }
        if (const auto it = obj->find("type_name"); it != obj->end()) {
            out.type_name = schema_read<std::string>(it->second);
        } else {
            out.type_name.reset();
        }
        if (const auto it = obj->find("record"); it != obj->end()) {
            out.record = schema_read<bool>(it->second);
        } else {
            out.record.reset();
        }
        if (const auto it = obj->find("direction"); it != obj->end()) {
            out.direction = schema_read<std::string>(it->second);
        } else {
            out.direction.reset();
        }
        if (const auto it = obj->find("qos_depth"); it != obj->end()) {
            out.qos_depth = schema_read<int64_t>(it->second);
        } else {
            out.qos_depth.reset();
        }
        if (const auto it = obj->find("qos_drop_policy"); it != obj->end()) {
            out.qos_drop_policy = schema_read<std::string>(it->second);
        } else {
            out.qos_drop_policy.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("name", schema_write(name));
        if (channel.has_value()) {
            obj.emplace("channel", schema_write(*channel));
        }
        if (type_name.has_value()) {
            obj.emplace("type_name", schema_write(*type_name));
        }
        if (record.has_value()) {
            obj.emplace("record", schema_write(*record));
        }
        if (direction.has_value()) {
            obj.emplace("direction", schema_write(*direction));
        }
        if (qos_depth.has_value()) {
            obj.emplace("qos_depth", schema_write(*qos_depth));
        }
        if (qos_drop_policy.has_value()) {
            obj.emplace("qos_drop_policy", schema_write(*qos_drop_policy));
        }
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct PortPlanView {
    PortPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> PortPlanView {
        return PortPlanView{PortPlan::from_value(value)};
    }

    auto Has_name() const -> bool {
        return true;
    }

    auto name() const -> const std::string& {
        return value.name;
    }

    auto Has_channel() const -> bool {
        return value.has_channel();
    }

    auto channel() const -> const std::string& {
        if (not value.channel.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: channel");
        }
        return *value.channel;
    }

    auto Has_type_name() const -> bool {
        return value.has_type_name();
    }

    auto type_name() const -> const std::string& {
        if (not value.type_name.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: type_name");
        }
        return *value.type_name;
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

    auto Has_direction() const -> bool {
        return value.has_direction();
    }

    auto direction() const -> const std::string& {
        if (not value.direction.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: direction");
        }
        return *value.direction;
    }

    auto Has_qos_depth() const -> bool {
        return value.has_qos_depth();
    }

    auto qos_depth() const -> const int64_t& {
        if (not value.qos_depth.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: qos_depth");
        }
        return *value.qos_depth;
    }

    auto Has_qos_drop_policy() const -> bool {
        return value.has_qos_drop_policy();
    }

    auto qos_drop_policy() const -> const std::string& {
        if (not value.qos_drop_policy.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: qos_drop_policy");
        }
        return *value.qos_drop_policy;
    }

};

struct ServicePortPlan {
    static constexpr uint32_t kFieldId_name = 1;
    static constexpr uint32_t kFieldId_channel = 2;
    static constexpr uint32_t kFieldId_request_type_name = 3;
    static constexpr uint32_t kFieldId_response_type_name = 4;
    static constexpr uint32_t kFieldId_role = 5;

    std::string name{};
    std::optional<std::string> channel{std::nullopt};
    std::optional<std::string> request_type_name{std::nullopt};
    std::optional<std::string> response_type_name{std::nullopt};
    std::optional<std::string> role{std::nullopt};

    auto has_channel() const -> bool {
        return channel.has_value();
    }

    auto has_request_type_name() const -> bool {
        return request_type_name.has_value();
    }

    auto has_response_type_name() const -> bool {
        return response_type_name.has_value();
    }

    auto has_role() const -> bool {
        return role.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ServicePortPlan {
        auto out = ServicePortPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("name"); it != obj->end()) {
            out.name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: name");
        }
        if (const auto it = obj->find("channel"); it != obj->end()) {
            out.channel = schema_read<std::string>(it->second);
        } else {
            out.channel.reset();
        }
        if (const auto it = obj->find("request_type_name"); it != obj->end()) {
            out.request_type_name = schema_read<std::string>(it->second);
        } else {
            out.request_type_name.reset();
        }
        if (const auto it = obj->find("response_type_name"); it != obj->end()) {
            out.response_type_name = schema_read<std::string>(it->second);
        } else {
            out.response_type_name.reset();
        }
        if (const auto it = obj->find("role"); it != obj->end()) {
            out.role = schema_read<std::string>(it->second);
        } else {
            out.role.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("name", schema_write(name));
        if (channel.has_value()) {
            obj.emplace("channel", schema_write(*channel));
        }
        if (request_type_name.has_value()) {
            obj.emplace("request_type_name", schema_write(*request_type_name));
        }
        if (response_type_name.has_value()) {
            obj.emplace("response_type_name", schema_write(*response_type_name));
        }
        if (role.has_value()) {
            obj.emplace("role", schema_write(*role));
        }
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct ServicePortPlanView {
    ServicePortPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ServicePortPlanView {
        return ServicePortPlanView{ServicePortPlan::from_value(value)};
    }

    auto Has_name() const -> bool {
        return true;
    }

    auto name() const -> const std::string& {
        return value.name;
    }

    auto Has_channel() const -> bool {
        return value.has_channel();
    }

    auto channel() const -> const std::string& {
        if (not value.channel.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: channel");
        }
        return *value.channel;
    }

    auto Has_request_type_name() const -> bool {
        return value.has_request_type_name();
    }

    auto request_type_name() const -> const std::string& {
        if (not value.request_type_name.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: request_type_name");
        }
        return *value.request_type_name;
    }

    auto Has_response_type_name() const -> bool {
        return value.has_response_type_name();
    }

    auto response_type_name() const -> const std::string& {
        if (not value.response_type_name.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: response_type_name");
        }
        return *value.response_type_name;
    }

    auto Has_role() const -> bool {
        return value.has_role();
    }

    auto role() const -> const std::string& {
        if (not value.role.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: role");
        }
        return *value.role;
    }

};

struct ComponentLoadPlan {
    static constexpr uint32_t kFieldId_module_name = 1;
    static constexpr uint32_t kFieldId_component_name = 2;
    static constexpr uint32_t kFieldId_component_group = 3;
    static constexpr uint32_t kFieldId_component_class = 4;
    static constexpr uint32_t kFieldId_component_library = 5;
    static constexpr uint32_t kFieldId_static_link = 6;
    static constexpr uint32_t kFieldId_priority = 7;
    static constexpr uint32_t kFieldId_component_conf_path = 8;
    static constexpr uint32_t kFieldId_module_conf_path = 9;
    static constexpr uint32_t kFieldId_module_dir = 10;
    static constexpr uint32_t kFieldId_module_config_dir = 11;
    static constexpr uint32_t kFieldId_config_file_path = 12;
    static constexpr uint32_t kFieldId_process_group = 13;
    static constexpr uint32_t kFieldId_scheduler_type = 14;
    static constexpr uint32_t kFieldId_executor_threads = 15;
    static constexpr uint32_t kFieldId_trigger = 16;
    static constexpr uint32_t kFieldId_inputs = 17;
    static constexpr uint32_t kFieldId_outputs = 18;
    static constexpr uint32_t kFieldId_services = 19;

    std::string module_name{};
    std::string component_name{};
    std::optional<std::string> component_group{std::nullopt};
    std::string component_class{};
    std::optional<std::string> component_library{std::nullopt};
    std::optional<bool> static_link{std::nullopt};
    std::optional<int64_t> priority{std::nullopt};
    std::optional<std::string> component_conf_path{std::nullopt};
    std::optional<std::string> module_conf_path{std::nullopt};
    std::optional<std::string> module_dir{std::nullopt};
    std::optional<std::string> module_config_dir{std::nullopt};
    std::optional<std::string> config_file_path{std::nullopt};
    std::optional<std::string> process_group{std::nullopt};
    std::optional<std::string> scheduler_type{std::nullopt};
    std::optional<int64_t> executor_threads{std::nullopt};
    std::optional<TriggerPlan> trigger{std::nullopt};
    std::optional<std::vector<PortPlan>> inputs{std::nullopt};
    std::optional<std::vector<PortPlan>> outputs{std::nullopt};
    std::optional<std::vector<ServicePortPlan>> services{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_component_group() const -> bool {
        return component_group.has_value();
    }

    auto has_component_library() const -> bool {
        return component_library.has_value();
    }

    auto has_static_link() const -> bool {
        return static_link.has_value();
    }

    auto has_priority() const -> bool {
        return priority.has_value();
    }

    auto has_component_conf_path() const -> bool {
        return component_conf_path.has_value();
    }

    auto has_module_conf_path() const -> bool {
        return module_conf_path.has_value();
    }

    auto has_module_dir() const -> bool {
        return module_dir.has_value();
    }

    auto has_module_config_dir() const -> bool {
        return module_config_dir.has_value();
    }

    auto has_config_file_path() const -> bool {
        return config_file_path.has_value();
    }

    auto has_process_group() const -> bool {
        return process_group.has_value();
    }

    auto has_scheduler_type() const -> bool {
        return scheduler_type.has_value();
    }

    auto has_executor_threads() const -> bool {
        return executor_threads.has_value();
    }

    auto has_trigger() const -> bool {
        return trigger.has_value();
    }

    auto has_inputs() const -> bool {
        return inputs.has_value();
    }

    auto has_outputs() const -> bool {
        return outputs.has_value();
    }

    auto has_services() const -> bool {
        return services.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ComponentLoadPlan {
        auto out = ComponentLoadPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("module_name"); it != obj->end()) {
            out.module_name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: module_name");
        }
        if (const auto it = obj->find("component_name"); it != obj->end()) {
            out.component_name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: component_name");
        }
        if (const auto it = obj->find("component_group"); it != obj->end()) {
            out.component_group = schema_read<std::string>(it->second);
        } else {
            out.component_group.reset();
        }
        if (const auto it = obj->find("component_class"); it != obj->end()) {
            out.component_class = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: component_class");
        }
        if (const auto it = obj->find("component_library"); it != obj->end()) {
            out.component_library = schema_read<std::string>(it->second);
        } else {
            out.component_library.reset();
        }
        if (const auto it = obj->find("static_link"); it != obj->end()) {
            out.static_link = schema_read<bool>(it->second);
        } else {
            out.static_link.reset();
        }
        if (const auto it = obj->find("priority"); it != obj->end()) {
            out.priority = schema_read<int64_t>(it->second);
        } else {
            out.priority.reset();
        }
        if (const auto it = obj->find("component_conf_path"); it != obj->end()) {
            out.component_conf_path = schema_read<std::string>(it->second);
        } else {
            out.component_conf_path.reset();
        }
        if (const auto it = obj->find("module_conf_path"); it != obj->end()) {
            out.module_conf_path = schema_read<std::string>(it->second);
        } else {
            out.module_conf_path.reset();
        }
        if (const auto it = obj->find("module_dir"); it != obj->end()) {
            out.module_dir = schema_read<std::string>(it->second);
        } else {
            out.module_dir.reset();
        }
        if (const auto it = obj->find("module_config_dir"); it != obj->end()) {
            out.module_config_dir = schema_read<std::string>(it->second);
        } else {
            out.module_config_dir.reset();
        }
        if (const auto it = obj->find("config_file_path"); it != obj->end()) {
            out.config_file_path = schema_read<std::string>(it->second);
        } else {
            out.config_file_path.reset();
        }
        if (const auto it = obj->find("process_group"); it != obj->end()) {
            out.process_group = schema_read<std::string>(it->second);
        } else {
            out.process_group.reset();
        }
        if (const auto it = obj->find("scheduler_type"); it != obj->end()) {
            out.scheduler_type = schema_read<std::string>(it->second);
        } else {
            out.scheduler_type.reset();
        }
        if (const auto it = obj->find("executor_threads"); it != obj->end()) {
            out.executor_threads = schema_read<int64_t>(it->second);
        } else {
            out.executor_threads.reset();
        }
        if (const auto it = obj->find("trigger"); it != obj->end()) {
            out.trigger = schema_read<TriggerPlan>(it->second);
        } else {
            out.trigger.reset();
        }
        if (const auto it = obj->find("inputs"); it != obj->end()) {
            out.inputs = schema_read<std::vector<PortPlan>>(it->second);
        } else {
            out.inputs.reset();
        }
        if (const auto it = obj->find("outputs"); it != obj->end()) {
            out.outputs = schema_read<std::vector<PortPlan>>(it->second);
        } else {
            out.outputs.reset();
        }
        if (const auto it = obj->find("services"); it != obj->end()) {
            out.services = schema_read<std::vector<ServicePortPlan>>(it->second);
        } else {
            out.services.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("module_name", schema_write(module_name));
        obj.emplace("component_name", schema_write(component_name));
        if (component_group.has_value()) {
            obj.emplace("component_group", schema_write(*component_group));
        }
        obj.emplace("component_class", schema_write(component_class));
        if (component_library.has_value()) {
            obj.emplace("component_library", schema_write(*component_library));
        }
        if (static_link.has_value()) {
            obj.emplace("static_link", schema_write(*static_link));
        }
        if (priority.has_value()) {
            obj.emplace("priority", schema_write(*priority));
        }
        if (component_conf_path.has_value()) {
            obj.emplace("component_conf_path", schema_write(*component_conf_path));
        }
        if (module_conf_path.has_value()) {
            obj.emplace("module_conf_path", schema_write(*module_conf_path));
        }
        if (module_dir.has_value()) {
            obj.emplace("module_dir", schema_write(*module_dir));
        }
        if (module_config_dir.has_value()) {
            obj.emplace("module_config_dir", schema_write(*module_config_dir));
        }
        if (config_file_path.has_value()) {
            obj.emplace("config_file_path", schema_write(*config_file_path));
        }
        if (process_group.has_value()) {
            obj.emplace("process_group", schema_write(*process_group));
        }
        if (scheduler_type.has_value()) {
            obj.emplace("scheduler_type", schema_write(*scheduler_type));
        }
        if (executor_threads.has_value()) {
            obj.emplace("executor_threads", schema_write(*executor_threads));
        }
        if (trigger.has_value()) {
            obj.emplace("trigger", schema_write(*trigger));
        }
        if (inputs.has_value()) {
            obj.emplace("inputs", schema_write(*inputs));
        }
        if (outputs.has_value()) {
            obj.emplace("outputs", schema_write(*outputs));
        }
        if (services.has_value()) {
            obj.emplace("services", schema_write(*services));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ComponentLoadPlan {
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

struct ComponentLoadPlanView {
    ComponentLoadPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ComponentLoadPlanView {
        return ComponentLoadPlanView{ComponentLoadPlan::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ComponentLoadPlanView {
        return ComponentLoadPlanView{ComponentLoadPlan::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_module_name() const -> bool {
        return true;
    }

    auto module_name() const -> const std::string& {
        return value.module_name;
    }

    auto Has_component_name() const -> bool {
        return true;
    }

    auto component_name() const -> const std::string& {
        return value.component_name;
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

    auto Has_component_class() const -> bool {
        return true;
    }

    auto component_class() const -> const std::string& {
        return value.component_class;
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

    auto Has_static_link() const -> bool {
        return value.has_static_link();
    }

    auto static_link() const -> const bool& {
        if (not value.static_link.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: static_link");
        }
        return *value.static_link;
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

    auto Has_component_conf_path() const -> bool {
        return value.has_component_conf_path();
    }

    auto component_conf_path() const -> const std::string& {
        if (not value.component_conf_path.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: component_conf_path");
        }
        return *value.component_conf_path;
    }

    auto Has_module_conf_path() const -> bool {
        return value.has_module_conf_path();
    }

    auto module_conf_path() const -> const std::string& {
        if (not value.module_conf_path.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: module_conf_path");
        }
        return *value.module_conf_path;
    }

    auto Has_module_dir() const -> bool {
        return value.has_module_dir();
    }

    auto module_dir() const -> const std::string& {
        if (not value.module_dir.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: module_dir");
        }
        return *value.module_dir;
    }

    auto Has_module_config_dir() const -> bool {
        return value.has_module_config_dir();
    }

    auto module_config_dir() const -> const std::string& {
        if (not value.module_config_dir.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: module_config_dir");
        }
        return *value.module_config_dir;
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

    auto Has_process_group() const -> bool {
        return value.has_process_group();
    }

    auto process_group() const -> const std::string& {
        if (not value.process_group.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: process_group");
        }
        return *value.process_group;
    }

    auto Has_scheduler_type() const -> bool {
        return value.has_scheduler_type();
    }

    auto scheduler_type() const -> const std::string& {
        if (not value.scheduler_type.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: scheduler_type");
        }
        return *value.scheduler_type;
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

    auto Has_trigger() const -> bool {
        return value.has_trigger();
    }

    auto trigger() const -> const TriggerPlan& {
        if (not value.trigger.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: trigger");
        }
        return *value.trigger;
    }

    auto Has_inputs() const -> bool {
        return value.has_inputs();
    }

    auto inputs() const -> const std::vector<PortPlan>& {
        if (not value.inputs.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: inputs");
        }
        return *value.inputs;
    }

    auto Has_outputs() const -> bool {
        return value.has_outputs();
    }

    auto outputs() const -> const std::vector<PortPlan>& {
        if (not value.outputs.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: outputs");
        }
        return *value.outputs;
    }

    auto Has_services() const -> bool {
        return value.has_services();
    }

    auto services() const -> const std::vector<ServicePortPlan>& {
        if (not value.services.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: services");
        }
        return *value.services;
    }

};

struct ComponentLoadPlanBuilder {
    ComponentLoadPlan value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const ComponentLoadPlan& value) -> ComponentLoadPlanBuilder {
        return ComponentLoadPlanBuilder{value, value.unknown_fields()};
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

using ComponentLoadPlanSchema = ComponentLoadPlan;

}  // namespace star::generated::runtime::component_load_plan

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_RUNTIME_COMPONENT_LOAD_PLAN_S_H__

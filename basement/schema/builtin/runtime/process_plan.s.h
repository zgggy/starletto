#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_RUNTIME_PROCESS_PLAN_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_RUNTIME_PROCESS_PLAN_S_H__

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

namespace star::generated::runtime::process_plan {

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
    out.name = "runtime.process_plan";
    out.version = 0;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_TaskSchedulePlan() -> std::vector<star::schema::FieldDescriptor> {
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
        field.name = "priority";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ChoreographyPlan() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "pool_threads";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "tasks";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "TaskSchedulePlan";
        field.optional = true;
        field.repeated = true;
        field.children = _build_fields_TaskSchedulePlan();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "cpu_affinity";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ProcessGroupPlan() -> std::vector<star::schema::FieldDescriptor> {
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
        field.name = "component_names";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "component_conf_paths";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "produced_channels";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "consumed_channels";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "scheduler_type";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 7;
        field.name = "choreography";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "ChoreographyPlan";
        field.optional = true;
        field.repeated = false;
        field.children = _build_fields_ChoreographyPlan();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ProcessPlan() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "groups";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "ProcessGroupPlan";
        field.optional = true;
        field.repeated = true;
        field.children = _build_fields_ProcessGroupPlan();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "";
    out.root_name = "ProcessPlan";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ProcessPlan();
    return out;
}

struct TaskSchedulePlan {
    static constexpr uint32_t kFieldId_name = 1;
    static constexpr uint32_t kFieldId_priority = 2;

    std::string name{};
    std::optional<int64_t> priority{std::nullopt};

    auto has_priority() const -> bool {
        return priority.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> TaskSchedulePlan {
        auto out = TaskSchedulePlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("name"); it != obj->end()) {
            out.name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: name");
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
        if (priority.has_value()) {
            obj.emplace("priority", schema_write(*priority));
        }
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct TaskSchedulePlanView {
    TaskSchedulePlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> TaskSchedulePlanView {
        return TaskSchedulePlanView{TaskSchedulePlan::from_value(value)};
    }

    auto Has_name() const -> bool {
        return true;
    }

    auto name() const -> const std::string& {
        return value.name;
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

struct ChoreographyPlan {
    static constexpr uint32_t kFieldId_pool_threads = 1;
    static constexpr uint32_t kFieldId_tasks = 2;
    static constexpr uint32_t kFieldId_cpu_affinity = 3;

    std::optional<int64_t> pool_threads{std::nullopt};
    std::optional<std::vector<TaskSchedulePlan>> tasks{std::nullopt};
    std::optional<std::string> cpu_affinity{std::nullopt};

    auto has_pool_threads() const -> bool {
        return pool_threads.has_value();
    }

    auto has_tasks() const -> bool {
        return tasks.has_value();
    }

    auto has_cpu_affinity() const -> bool {
        return cpu_affinity.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ChoreographyPlan {
        auto out = ChoreographyPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("pool_threads"); it != obj->end()) {
            out.pool_threads = schema_read<int64_t>(it->second);
        } else {
            out.pool_threads.reset();
        }
        if (const auto it = obj->find("tasks"); it != obj->end()) {
            out.tasks = schema_read<std::vector<TaskSchedulePlan>>(it->second);
        } else {
            out.tasks.reset();
        }
        if (const auto it = obj->find("cpu_affinity"); it != obj->end()) {
            out.cpu_affinity = schema_read<std::string>(it->second);
        } else {
            out.cpu_affinity.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (pool_threads.has_value()) {
            obj.emplace("pool_threads", schema_write(*pool_threads));
        }
        if (tasks.has_value()) {
            obj.emplace("tasks", schema_write(*tasks));
        }
        if (cpu_affinity.has_value()) {
            obj.emplace("cpu_affinity", schema_write(*cpu_affinity));
        }
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct ChoreographyPlanView {
    ChoreographyPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ChoreographyPlanView {
        return ChoreographyPlanView{ChoreographyPlan::from_value(value)};
    }

    auto Has_pool_threads() const -> bool {
        return value.has_pool_threads();
    }

    auto pool_threads() const -> const int64_t& {
        if (not value.pool_threads.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: pool_threads");
        }
        return *value.pool_threads;
    }

    auto Has_tasks() const -> bool {
        return value.has_tasks();
    }

    auto tasks() const -> const std::vector<TaskSchedulePlan>& {
        if (not value.tasks.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: tasks");
        }
        return *value.tasks;
    }

    auto Has_cpu_affinity() const -> bool {
        return value.has_cpu_affinity();
    }

    auto cpu_affinity() const -> const std::string& {
        if (not value.cpu_affinity.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: cpu_affinity");
        }
        return *value.cpu_affinity;
    }

};

struct ProcessGroupPlan {
    static constexpr uint32_t kFieldId_name = 1;
    static constexpr uint32_t kFieldId_component_names = 2;
    static constexpr uint32_t kFieldId_component_conf_paths = 3;
    static constexpr uint32_t kFieldId_produced_channels = 4;
    static constexpr uint32_t kFieldId_consumed_channels = 5;
    static constexpr uint32_t kFieldId_scheduler_type = 6;
    static constexpr uint32_t kFieldId_choreography = 7;

    std::string name{};
    std::optional<std::vector<std::string>> component_names{std::nullopt};
    std::optional<std::vector<std::string>> component_conf_paths{std::nullopt};
    std::optional<std::vector<std::string>> produced_channels{std::nullopt};
    std::optional<std::vector<std::string>> consumed_channels{std::nullopt};
    std::optional<std::string> scheduler_type{std::nullopt};
    std::optional<ChoreographyPlan> choreography{std::nullopt};

    auto has_component_names() const -> bool {
        return component_names.has_value();
    }

    auto has_component_conf_paths() const -> bool {
        return component_conf_paths.has_value();
    }

    auto has_produced_channels() const -> bool {
        return produced_channels.has_value();
    }

    auto has_consumed_channels() const -> bool {
        return consumed_channels.has_value();
    }

    auto has_scheduler_type() const -> bool {
        return scheduler_type.has_value();
    }

    auto has_choreography() const -> bool {
        return choreography.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ProcessGroupPlan {
        auto out = ProcessGroupPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("name"); it != obj->end()) {
            out.name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: name");
        }
        if (const auto it = obj->find("component_names"); it != obj->end()) {
            out.component_names = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.component_names.reset();
        }
        if (const auto it = obj->find("component_conf_paths"); it != obj->end()) {
            out.component_conf_paths = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.component_conf_paths.reset();
        }
        if (const auto it = obj->find("produced_channels"); it != obj->end()) {
            out.produced_channels = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.produced_channels.reset();
        }
        if (const auto it = obj->find("consumed_channels"); it != obj->end()) {
            out.consumed_channels = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.consumed_channels.reset();
        }
        if (const auto it = obj->find("scheduler_type"); it != obj->end()) {
            out.scheduler_type = schema_read<std::string>(it->second);
        } else {
            out.scheduler_type.reset();
        }
        if (const auto it = obj->find("choreography"); it != obj->end()) {
            out.choreography = schema_read<ChoreographyPlan>(it->second);
        } else {
            out.choreography.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("name", schema_write(name));
        if (component_names.has_value()) {
            obj.emplace("component_names", schema_write(*component_names));
        }
        if (component_conf_paths.has_value()) {
            obj.emplace("component_conf_paths", schema_write(*component_conf_paths));
        }
        if (produced_channels.has_value()) {
            obj.emplace("produced_channels", schema_write(*produced_channels));
        }
        if (consumed_channels.has_value()) {
            obj.emplace("consumed_channels", schema_write(*consumed_channels));
        }
        if (scheduler_type.has_value()) {
            obj.emplace("scheduler_type", schema_write(*scheduler_type));
        }
        if (choreography.has_value()) {
            obj.emplace("choreography", schema_write(*choreography));
        }
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct ProcessGroupPlanView {
    ProcessGroupPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ProcessGroupPlanView {
        return ProcessGroupPlanView{ProcessGroupPlan::from_value(value)};
    }

    auto Has_name() const -> bool {
        return true;
    }

    auto name() const -> const std::string& {
        return value.name;
    }

    auto Has_component_names() const -> bool {
        return value.has_component_names();
    }

    auto component_names() const -> const std::vector<std::string>& {
        if (not value.component_names.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: component_names");
        }
        return *value.component_names;
    }

    auto Has_component_conf_paths() const -> bool {
        return value.has_component_conf_paths();
    }

    auto component_conf_paths() const -> const std::vector<std::string>& {
        if (not value.component_conf_paths.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: component_conf_paths");
        }
        return *value.component_conf_paths;
    }

    auto Has_produced_channels() const -> bool {
        return value.has_produced_channels();
    }

    auto produced_channels() const -> const std::vector<std::string>& {
        if (not value.produced_channels.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: produced_channels");
        }
        return *value.produced_channels;
    }

    auto Has_consumed_channels() const -> bool {
        return value.has_consumed_channels();
    }

    auto consumed_channels() const -> const std::vector<std::string>& {
        if (not value.consumed_channels.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: consumed_channels");
        }
        return *value.consumed_channels;
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

    auto Has_choreography() const -> bool {
        return value.has_choreography();
    }

    auto choreography() const -> const ChoreographyPlan& {
        if (not value.choreography.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: choreography");
        }
        return *value.choreography;
    }

};

struct ProcessPlan {
    static constexpr uint32_t kFieldId_groups = 1;

    std::optional<std::vector<ProcessGroupPlan>> groups{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_groups() const -> bool {
        return groups.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ProcessPlan {
        auto out = ProcessPlan{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("groups"); it != obj->end()) {
            out.groups = schema_read<std::vector<ProcessGroupPlan>>(it->second);
        } else {
            out.groups.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (groups.has_value()) {
            obj.emplace("groups", schema_write(*groups));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ProcessPlan {
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

struct ProcessPlanView {
    ProcessPlan value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ProcessPlanView {
        return ProcessPlanView{ProcessPlan::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ProcessPlanView {
        return ProcessPlanView{ProcessPlan::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_groups() const -> bool {
        return value.has_groups();
    }

    auto groups() const -> const std::vector<ProcessGroupPlan>& {
        if (not value.groups.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: groups");
        }
        return *value.groups;
    }

};

struct ProcessPlanBuilder {
    ProcessPlan value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const ProcessPlan& value) -> ProcessPlanBuilder {
        return ProcessPlanBuilder{value, value.unknown_fields()};
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

using ProcessPlanSchema = ProcessPlan;

}  // namespace star::generated::runtime::process_plan

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_BASEMENT_SCHEMA_BUILTIN_RUNTIME_PROCESS_PLAN_S_H__

#ifndef __RUNTIME_PROCESS_PLAN__
#define __RUNTIME_PROCESS_PLAN__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::generated::runtime::process_plan {

using star::schema::schema_read;
using star::schema::schema_write;

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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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

struct TaskSchedulePlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> TaskSchedulePlan;
};

struct TaskSchedulePlanView {
    static constexpr size_t kFieldCount = 2;

    TaskSchedulePlanView() = default;
    explicit TaskSchedulePlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit TaskSchedulePlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_name = 0;
    static constexpr size_t kSlotIndex_priority = 1;

    auto Has_name() const -> bool {
        return not view_.is_null(0);
    }

    auto name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_priority() const -> bool {
        return not view_.is_null(1);
    }

    auto priority() const -> int64_t {
        auto v = view_.get_int64(1);
        return v.has_value() ? *v : int64_t{0};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct TaskSchedulePlanBuilder {
    static constexpr size_t kFieldCount = 2;

    TaskSchedulePlanBuilder() : writer_(2) {}

    void set_name(std::string_view value) { writer_.set_string(0, value); }
    void set_priority(int64_t value) { writer_.set_int64(1, value); }

    void clear_priority() { writer_.set_null(1); }

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

inline auto TaskSchedulePlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = TaskSchedulePlanBuilder();
    builder.set_name(name);
    if (priority.has_value()) {
        builder.set_priority(*priority);
    } else {
        builder.clear_priority();
    }
    return builder.Serialize();
}

inline auto TaskSchedulePlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = TaskSchedulePlanBuilder();
    builder.set_name(name);
    if (priority.has_value()) {
        builder.set_priority(*priority);
    } else {
        builder.clear_priority();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto TaskSchedulePlan::from_fixed_layout(std::span<const uint8_t> data) -> TaskSchedulePlan {
    auto view = TaskSchedulePlanView(data.data(), data.size());
    auto out = TaskSchedulePlan();
    out.name = view.name();
    if (view.Has_priority()) { out.priority = view.priority(); }
    return out;
}

struct ChoreographyPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ChoreographyPlan;
};

struct ChoreographyPlanView {
    static constexpr size_t kFieldCount = 3;

    ChoreographyPlanView() = default;
    explicit ChoreographyPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ChoreographyPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_pool_threads = 0;
    static constexpr size_t kSlotIndex_tasks = 1;
    static constexpr size_t kSlotIndex_cpu_affinity = 2;

    auto Has_pool_threads() const -> bool {
        return not view_.is_null(0);
    }

    auto pool_threads() const -> int64_t {
        auto v = view_.get_int64(0);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_tasks() const -> bool {
        return not view_.is_null(1);
    }

    auto tasks_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_array_bytes(1);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_cpu_affinity() const -> bool {
        return not view_.is_null(2);
    }

    auto cpu_affinity() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ChoreographyPlanBuilder {
    static constexpr size_t kFieldCount = 3;

    ChoreographyPlanBuilder() : writer_(3) {}

    void set_pool_threads(int64_t value) { writer_.set_int64(0, value); }
    void set_tasks_bytes(std::span<const uint8_t> value) { writer_.set_array(1, value); }
    void set_cpu_affinity(std::string_view value) { writer_.set_string(2, value); }

    void clear_pool_threads() { writer_.set_null(0); }
    void clear_tasks() { writer_.set_null(1); }
    void clear_cpu_affinity() { writer_.set_null(2); }

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

inline auto ChoreographyPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ChoreographyPlanBuilder();
    if (pool_threads.has_value()) {
        builder.set_pool_threads(*pool_threads);
    } else {
        builder.clear_pool_threads();
    }
    if (tasks.has_value()) {
        { auto bytes = star::transport::encode_object_array(*tasks); builder.set_tasks_bytes(bytes); }
    } else {
        builder.clear_tasks();
    }
    if (cpu_affinity.has_value()) {
        builder.set_cpu_affinity(*cpu_affinity);
    } else {
        builder.clear_cpu_affinity();
    }
    return builder.Serialize();
}

inline auto ChoreographyPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ChoreographyPlanBuilder();
    if (pool_threads.has_value()) {
        builder.set_pool_threads(*pool_threads);
    } else {
        builder.clear_pool_threads();
    }
    if (tasks.has_value()) {
        { auto bytes = star::transport::encode_object_array(*tasks); builder.set_tasks_bytes(bytes); }
    } else {
        builder.clear_tasks();
    }
    if (cpu_affinity.has_value()) {
        builder.set_cpu_affinity(*cpu_affinity);
    } else {
        builder.clear_cpu_affinity();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ChoreographyPlan::from_fixed_layout(std::span<const uint8_t> data) -> ChoreographyPlan {
    auto view = ChoreographyPlanView(data.data(), data.size());
    auto out = ChoreographyPlan();
    if (view.Has_pool_threads()) { out.pool_threads = view.pool_threads(); }
    if (view.Has_tasks()) { auto bytes = view.tasks_bytes(); out.tasks = star::transport::decode_object_array<TaskSchedulePlan>(bytes); }
    if (view.Has_cpu_affinity()) { out.cpu_affinity = view.cpu_affinity(); }
    return out;
}

struct ProcessGroupPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ProcessGroupPlan;
};

struct ProcessGroupPlanView {
    static constexpr size_t kFieldCount = 7;

    ProcessGroupPlanView() = default;
    explicit ProcessGroupPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ProcessGroupPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_name = 0;
    static constexpr size_t kSlotIndex_component_names = 1;
    static constexpr size_t kSlotIndex_component_conf_paths = 2;
    static constexpr size_t kSlotIndex_produced_channels = 3;
    static constexpr size_t kSlotIndex_consumed_channels = 4;
    static constexpr size_t kSlotIndex_scheduler_type = 5;
    static constexpr size_t kSlotIndex_choreography = 6;

    auto Has_name() const -> bool {
        return not view_.is_null(0);
    }

    auto name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_component_names() const -> bool {
        return not view_.is_null(1);
    }

    auto component_names() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(1);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto Has_component_conf_paths() const -> bool {
        return not view_.is_null(2);
    }

    auto component_conf_paths() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(2);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto Has_produced_channels() const -> bool {
        return not view_.is_null(3);
    }

    auto produced_channels() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(3);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto Has_consumed_channels() const -> bool {
        return not view_.is_null(4);
    }

    auto consumed_channels() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(4);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto Has_scheduler_type() const -> bool {
        return not view_.is_null(5);
    }

    auto scheduler_type() const -> std::string_view {
        auto v = view_.get_string(5);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_choreography() const -> bool {
        return not view_.is_null(6);
    }

    auto choreography_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(6);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ProcessGroupPlanBuilder {
    static constexpr size_t kFieldCount = 7;

    ProcessGroupPlanBuilder() : writer_(7) {}

    void set_name(std::string_view value) { writer_.set_string(0, value); }
    void set_component_names(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(1, bytes); }
    void set_component_conf_paths(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(2, bytes); }
    void set_produced_channels(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(3, bytes); }
    void set_consumed_channels(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(4, bytes); }
    void set_scheduler_type(std::string_view value) { writer_.set_string(5, value); }
    void set_choreography_bytes(std::span<const uint8_t> value) { writer_.set_object(6, value); }

    void clear_component_names() { writer_.set_null(1); }
    void clear_component_conf_paths() { writer_.set_null(2); }
    void clear_produced_channels() { writer_.set_null(3); }
    void clear_consumed_channels() { writer_.set_null(4); }
    void clear_scheduler_type() { writer_.set_null(5); }
    void clear_choreography() { writer_.set_null(6); }

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

inline auto ProcessGroupPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ProcessGroupPlanBuilder();
    builder.set_name(name);
    if (component_names.has_value()) {
        builder.set_component_names(*component_names);
    } else {
        builder.clear_component_names();
    }
    if (component_conf_paths.has_value()) {
        builder.set_component_conf_paths(*component_conf_paths);
    } else {
        builder.clear_component_conf_paths();
    }
    if (produced_channels.has_value()) {
        builder.set_produced_channels(*produced_channels);
    } else {
        builder.clear_produced_channels();
    }
    if (consumed_channels.has_value()) {
        builder.set_consumed_channels(*consumed_channels);
    } else {
        builder.clear_consumed_channels();
    }
    if (scheduler_type.has_value()) {
        builder.set_scheduler_type(*scheduler_type);
    } else {
        builder.clear_scheduler_type();
    }
    if (choreography.has_value()) {
        { auto bytes = choreography->encode_fixed_layout(); builder.set_choreography_bytes(bytes); }
    } else {
        builder.clear_choreography();
    }
    return builder.Serialize();
}

inline auto ProcessGroupPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ProcessGroupPlanBuilder();
    builder.set_name(name);
    if (component_names.has_value()) {
        builder.set_component_names(*component_names);
    } else {
        builder.clear_component_names();
    }
    if (component_conf_paths.has_value()) {
        builder.set_component_conf_paths(*component_conf_paths);
    } else {
        builder.clear_component_conf_paths();
    }
    if (produced_channels.has_value()) {
        builder.set_produced_channels(*produced_channels);
    } else {
        builder.clear_produced_channels();
    }
    if (consumed_channels.has_value()) {
        builder.set_consumed_channels(*consumed_channels);
    } else {
        builder.clear_consumed_channels();
    }
    if (scheduler_type.has_value()) {
        builder.set_scheduler_type(*scheduler_type);
    } else {
        builder.clear_scheduler_type();
    }
    if (choreography.has_value()) {
        { auto bytes = choreography->encode_fixed_layout(); builder.set_choreography_bytes(bytes); }
    } else {
        builder.clear_choreography();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ProcessGroupPlan::from_fixed_layout(std::span<const uint8_t> data) -> ProcessGroupPlan {
    auto view = ProcessGroupPlanView(data.data(), data.size());
    auto out = ProcessGroupPlan();
    out.name = view.name();
    if (view.Has_component_names()) { out.component_names = view.component_names(); }
    if (view.Has_component_conf_paths()) { out.component_conf_paths = view.component_conf_paths(); }
    if (view.Has_produced_channels()) { out.produced_channels = view.produced_channels(); }
    if (view.Has_consumed_channels()) { out.consumed_channels = view.consumed_channels(); }
    if (view.Has_scheduler_type()) { out.scheduler_type = view.scheduler_type(); }
    if (view.Has_choreography()) { auto bytes = view.choreography_bytes(); out.choreography = ChoreographyPlan::from_fixed_layout(bytes); }
    return out;
}

struct ProcessPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ProcessPlan;
};

struct ProcessPlanView {
    static constexpr size_t kFieldCount = 1;

    ProcessPlanView() = default;
    explicit ProcessPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ProcessPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ProcessPlanView {
        return ProcessPlanView(bytes);
    }

    static constexpr size_t kSlotIndex_groups = 0;

    auto Has_groups() const -> bool {
        return not view_.is_null(0);
    }

    auto groups_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_array_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ProcessPlanBuilder {
    static constexpr size_t kFieldCount = 1;

    ProcessPlanBuilder() : writer_(1) {}

    void set_groups_bytes(std::span<const uint8_t> value) { writer_.set_array(0, value); }

    void clear_groups() { writer_.set_null(0); }

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

inline auto ProcessPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ProcessPlanBuilder();
    if (groups.has_value()) {
        { auto bytes = star::transport::encode_object_array(*groups); builder.set_groups_bytes(bytes); }
    } else {
        builder.clear_groups();
    }
    return builder.Serialize();
}

inline auto ProcessPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ProcessPlanBuilder();
    if (groups.has_value()) {
        { auto bytes = star::transport::encode_object_array(*groups); builder.set_groups_bytes(bytes); }
    } else {
        builder.clear_groups();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ProcessPlan::from_fixed_layout(std::span<const uint8_t> data) -> ProcessPlan {
    auto view = ProcessPlanView(data.data(), data.size());
    auto out = ProcessPlan();
    if (view.Has_groups()) { auto bytes = view.groups_bytes(); out.groups = star::transport::decode_object_array<ProcessGroupPlan>(bytes); }
    return out;
}

using ProcessPlanSchema = ProcessPlan;

}  // namespace star::generated::runtime::process_plan

#endif // __RUNTIME_PROCESS_PLAN__

#ifndef __RUNTIME_COMPONENT_LOAD_PLAN__
#define __RUNTIME_COMPONENT_LOAD_PLAN__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::generated::runtime::component_load_plan {

using star::schema::schema_read;
using star::schema::schema_write;

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
        field.explicit_required = false;
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
        field.explicit_required = false;
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

inline auto _build_fields_PortPlan() -> std::vector<star::schema::FieldDescriptor> {
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
        field.name = "channel";
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
        field.name = "type_name";
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
        field.name = "record";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = true;
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.id = 6;
        field.name = "static_link";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = true;
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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

struct TriggerPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> TriggerPlan;
};

struct TriggerPlanView {
    static constexpr size_t kFieldCount = 4;

    TriggerPlanView() = default;
    explicit TriggerPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit TriggerPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_type = 0;
    static constexpr size_t kSlotIndex_task_name = 1;
    static constexpr size_t kSlotIndex_cycle_ms = 2;
    static constexpr size_t kSlotIndex_readers = 3;

    auto Has_type() const -> bool {
        return not view_.is_null(0);
    }

    auto type() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_task_name() const -> bool {
        return not view_.is_null(1);
    }

    auto task_name() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_cycle_ms() const -> bool {
        return not view_.is_null(2);
    }

    auto cycle_ms() const -> int64_t {
        auto v = view_.get_int64(2);
        return v.has_value() ? *v : int64_t{0};
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

struct TriggerPlanBuilder {
    static constexpr size_t kFieldCount = 4;

    TriggerPlanBuilder() : writer_(4) {}

    void set_type(std::string_view value) { writer_.set_string(0, value); }
    void set_task_name(std::string_view value) { writer_.set_string(1, value); }
    void set_cycle_ms(int64_t value) { writer_.set_int64(2, value); }
    void set_readers(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(3, bytes); }

    void clear_type() { writer_.set_null(0); }
    void clear_task_name() { writer_.set_null(1); }
    void clear_cycle_ms() { writer_.set_null(2); }
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

inline auto TriggerPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = TriggerPlanBuilder();
    if (type.has_value()) {
        builder.set_type(*type);
    } else {
        builder.clear_type();
    }
    if (task_name.has_value()) {
        builder.set_task_name(*task_name);
    } else {
        builder.clear_task_name();
    }
    if (cycle_ms.has_value()) {
        builder.set_cycle_ms(*cycle_ms);
    } else {
        builder.clear_cycle_ms();
    }
    if (readers.has_value()) {
        builder.set_readers(*readers);
    } else {
        builder.clear_readers();
    }
    return builder.Serialize();
}

inline auto TriggerPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = TriggerPlanBuilder();
    if (type.has_value()) {
        builder.set_type(*type);
    } else {
        builder.clear_type();
    }
    if (task_name.has_value()) {
        builder.set_task_name(*task_name);
    } else {
        builder.clear_task_name();
    }
    if (cycle_ms.has_value()) {
        builder.set_cycle_ms(*cycle_ms);
    } else {
        builder.clear_cycle_ms();
    }
    if (readers.has_value()) {
        builder.set_readers(*readers);
    } else {
        builder.clear_readers();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto TriggerPlan::from_fixed_layout(std::span<const uint8_t> data) -> TriggerPlan {
    auto view = TriggerPlanView(data.data(), data.size());
    auto out = TriggerPlan();
    if (view.Has_type()) { out.type = view.type(); }
    if (view.Has_task_name()) { out.task_name = view.task_name(); }
    if (view.Has_cycle_ms()) { out.cycle_ms = view.cycle_ms(); }
    if (view.Has_readers()) { out.readers = view.readers(); }
    return out;
}

struct PortPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> PortPlan;
};

struct PortPlanView {
    static constexpr size_t kFieldCount = 7;

    PortPlanView() = default;
    explicit PortPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit PortPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_name = 0;
    static constexpr size_t kSlotIndex_channel = 1;
    static constexpr size_t kSlotIndex_type_name = 2;
    static constexpr size_t kSlotIndex_record = 3;
    static constexpr size_t kSlotIndex_direction = 4;
    static constexpr size_t kSlotIndex_qos_depth = 5;
    static constexpr size_t kSlotIndex_qos_drop_policy = 6;

    auto Has_name() const -> bool {
        return not view_.is_null(0);
    }

    auto name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_channel() const -> bool {
        return not view_.is_null(1);
    }

    auto channel() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_type_name() const -> bool {
        return not view_.is_null(2);
    }

    auto type_name() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_record() const -> bool {
        return not view_.is_null(3);
    }

    auto record() const -> bool {
        auto v = view_.get_bool(3);
        return v.has_value() ? *v : false;
    }

    auto Has_direction() const -> bool {
        return not view_.is_null(4);
    }

    auto direction() const -> std::string_view {
        auto v = view_.get_string(4);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_qos_depth() const -> bool {
        return not view_.is_null(5);
    }

    auto qos_depth() const -> int64_t {
        auto v = view_.get_int64(5);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_qos_drop_policy() const -> bool {
        return not view_.is_null(6);
    }

    auto qos_drop_policy() const -> std::string_view {
        auto v = view_.get_string(6);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct PortPlanBuilder {
    static constexpr size_t kFieldCount = 7;

    PortPlanBuilder() : writer_(7) {}

    void set_name(std::string_view value) { writer_.set_string(0, value); }
    void set_channel(std::string_view value) { writer_.set_string(1, value); }
    void set_type_name(std::string_view value) { writer_.set_string(2, value); }
    void set_record(bool value) { writer_.set_bool(3, value); }
    void set_direction(std::string_view value) { writer_.set_string(4, value); }
    void set_qos_depth(int64_t value) { writer_.set_int64(5, value); }
    void set_qos_drop_policy(std::string_view value) { writer_.set_string(6, value); }

    void clear_channel() { writer_.set_null(1); }
    void clear_type_name() { writer_.set_null(2); }
    void clear_record() { writer_.set_null(3); }
    void clear_direction() { writer_.set_null(4); }
    void clear_qos_depth() { writer_.set_null(5); }
    void clear_qos_drop_policy() { writer_.set_null(6); }

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

inline auto PortPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = PortPlanBuilder();
    builder.set_name(name);
    if (channel.has_value()) {
        builder.set_channel(*channel);
    } else {
        builder.clear_channel();
    }
    if (type_name.has_value()) {
        builder.set_type_name(*type_name);
    } else {
        builder.clear_type_name();
    }
    if (record.has_value()) {
        builder.set_record(*record);
    } else {
        builder.clear_record();
    }
    if (direction.has_value()) {
        builder.set_direction(*direction);
    } else {
        builder.clear_direction();
    }
    if (qos_depth.has_value()) {
        builder.set_qos_depth(*qos_depth);
    } else {
        builder.clear_qos_depth();
    }
    if (qos_drop_policy.has_value()) {
        builder.set_qos_drop_policy(*qos_drop_policy);
    } else {
        builder.clear_qos_drop_policy();
    }
    return builder.Serialize();
}

inline auto PortPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = PortPlanBuilder();
    builder.set_name(name);
    if (channel.has_value()) {
        builder.set_channel(*channel);
    } else {
        builder.clear_channel();
    }
    if (type_name.has_value()) {
        builder.set_type_name(*type_name);
    } else {
        builder.clear_type_name();
    }
    if (record.has_value()) {
        builder.set_record(*record);
    } else {
        builder.clear_record();
    }
    if (direction.has_value()) {
        builder.set_direction(*direction);
    } else {
        builder.clear_direction();
    }
    if (qos_depth.has_value()) {
        builder.set_qos_depth(*qos_depth);
    } else {
        builder.clear_qos_depth();
    }
    if (qos_drop_policy.has_value()) {
        builder.set_qos_drop_policy(*qos_drop_policy);
    } else {
        builder.clear_qos_drop_policy();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto PortPlan::from_fixed_layout(std::span<const uint8_t> data) -> PortPlan {
    auto view = PortPlanView(data.data(), data.size());
    auto out = PortPlan();
    out.name = view.name();
    if (view.Has_channel()) { out.channel = view.channel(); }
    if (view.Has_type_name()) { out.type_name = view.type_name(); }
    if (view.Has_record()) { out.record = view.record(); }
    if (view.Has_direction()) { out.direction = view.direction(); }
    if (view.Has_qos_depth()) { out.qos_depth = view.qos_depth(); }
    if (view.Has_qos_drop_policy()) { out.qos_drop_policy = view.qos_drop_policy(); }
    return out;
}

struct ServicePortPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ServicePortPlan;
};

struct ServicePortPlanView {
    static constexpr size_t kFieldCount = 5;

    ServicePortPlanView() = default;
    explicit ServicePortPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ServicePortPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_name = 0;
    static constexpr size_t kSlotIndex_channel = 1;
    static constexpr size_t kSlotIndex_request_type_name = 2;
    static constexpr size_t kSlotIndex_response_type_name = 3;
    static constexpr size_t kSlotIndex_role = 4;

    auto Has_name() const -> bool {
        return not view_.is_null(0);
    }

    auto name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_channel() const -> bool {
        return not view_.is_null(1);
    }

    auto channel() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_request_type_name() const -> bool {
        return not view_.is_null(2);
    }

    auto request_type_name() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_response_type_name() const -> bool {
        return not view_.is_null(3);
    }

    auto response_type_name() const -> std::string_view {
        auto v = view_.get_string(3);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_role() const -> bool {
        return not view_.is_null(4);
    }

    auto role() const -> std::string_view {
        auto v = view_.get_string(4);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ServicePortPlanBuilder {
    static constexpr size_t kFieldCount = 5;

    ServicePortPlanBuilder() : writer_(5) {}

    void set_name(std::string_view value) { writer_.set_string(0, value); }
    void set_channel(std::string_view value) { writer_.set_string(1, value); }
    void set_request_type_name(std::string_view value) { writer_.set_string(2, value); }
    void set_response_type_name(std::string_view value) { writer_.set_string(3, value); }
    void set_role(std::string_view value) { writer_.set_string(4, value); }

    void clear_channel() { writer_.set_null(1); }
    void clear_request_type_name() { writer_.set_null(2); }
    void clear_response_type_name() { writer_.set_null(3); }
    void clear_role() { writer_.set_null(4); }

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

inline auto ServicePortPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ServicePortPlanBuilder();
    builder.set_name(name);
    if (channel.has_value()) {
        builder.set_channel(*channel);
    } else {
        builder.clear_channel();
    }
    if (request_type_name.has_value()) {
        builder.set_request_type_name(*request_type_name);
    } else {
        builder.clear_request_type_name();
    }
    if (response_type_name.has_value()) {
        builder.set_response_type_name(*response_type_name);
    } else {
        builder.clear_response_type_name();
    }
    if (role.has_value()) {
        builder.set_role(*role);
    } else {
        builder.clear_role();
    }
    return builder.Serialize();
}

inline auto ServicePortPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ServicePortPlanBuilder();
    builder.set_name(name);
    if (channel.has_value()) {
        builder.set_channel(*channel);
    } else {
        builder.clear_channel();
    }
    if (request_type_name.has_value()) {
        builder.set_request_type_name(*request_type_name);
    } else {
        builder.clear_request_type_name();
    }
    if (response_type_name.has_value()) {
        builder.set_response_type_name(*response_type_name);
    } else {
        builder.clear_response_type_name();
    }
    if (role.has_value()) {
        builder.set_role(*role);
    } else {
        builder.clear_role();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ServicePortPlan::from_fixed_layout(std::span<const uint8_t> data) -> ServicePortPlan {
    auto view = ServicePortPlanView(data.data(), data.size());
    auto out = ServicePortPlan();
    out.name = view.name();
    if (view.Has_channel()) { out.channel = view.channel(); }
    if (view.Has_request_type_name()) { out.request_type_name = view.request_type_name(); }
    if (view.Has_response_type_name()) { out.response_type_name = view.response_type_name(); }
    if (view.Has_role()) { out.role = view.role(); }
    return out;
}

struct ComponentLoadPlanBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ComponentLoadPlan;
};

struct ComponentLoadPlanView {
    static constexpr size_t kFieldCount = 19;

    ComponentLoadPlanView() = default;
    explicit ComponentLoadPlanView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ComponentLoadPlanView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ComponentLoadPlanView {
        return ComponentLoadPlanView(bytes);
    }

    static constexpr size_t kSlotIndex_module_name = 0;
    static constexpr size_t kSlotIndex_component_name = 1;
    static constexpr size_t kSlotIndex_component_group = 2;
    static constexpr size_t kSlotIndex_component_class = 3;
    static constexpr size_t kSlotIndex_component_library = 4;
    static constexpr size_t kSlotIndex_static_link = 5;
    static constexpr size_t kSlotIndex_priority = 6;
    static constexpr size_t kSlotIndex_component_conf_path = 7;
    static constexpr size_t kSlotIndex_module_conf_path = 8;
    static constexpr size_t kSlotIndex_module_dir = 9;
    static constexpr size_t kSlotIndex_module_config_dir = 10;
    static constexpr size_t kSlotIndex_config_file_path = 11;
    static constexpr size_t kSlotIndex_process_group = 12;
    static constexpr size_t kSlotIndex_scheduler_type = 13;
    static constexpr size_t kSlotIndex_executor_threads = 14;
    static constexpr size_t kSlotIndex_trigger = 15;
    static constexpr size_t kSlotIndex_inputs = 16;
    static constexpr size_t kSlotIndex_outputs = 17;
    static constexpr size_t kSlotIndex_services = 18;

    auto Has_module_name() const -> bool {
        return not view_.is_null(0);
    }

    auto module_name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_component_name() const -> bool {
        return not view_.is_null(1);
    }

    auto component_name() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_component_group() const -> bool {
        return not view_.is_null(2);
    }

    auto component_group() const -> std::string_view {
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

    auto Has_component_library() const -> bool {
        return not view_.is_null(4);
    }

    auto component_library() const -> std::string_view {
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

    auto Has_priority() const -> bool {
        return not view_.is_null(6);
    }

    auto priority() const -> int64_t {
        auto v = view_.get_int64(6);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_component_conf_path() const -> bool {
        return not view_.is_null(7);
    }

    auto component_conf_path() const -> std::string_view {
        auto v = view_.get_string(7);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_module_conf_path() const -> bool {
        return not view_.is_null(8);
    }

    auto module_conf_path() const -> std::string_view {
        auto v = view_.get_string(8);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_module_dir() const -> bool {
        return not view_.is_null(9);
    }

    auto module_dir() const -> std::string_view {
        auto v = view_.get_string(9);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_module_config_dir() const -> bool {
        return not view_.is_null(10);
    }

    auto module_config_dir() const -> std::string_view {
        auto v = view_.get_string(10);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_config_file_path() const -> bool {
        return not view_.is_null(11);
    }

    auto config_file_path() const -> std::string_view {
        auto v = view_.get_string(11);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_process_group() const -> bool {
        return not view_.is_null(12);
    }

    auto process_group() const -> std::string_view {
        auto v = view_.get_string(12);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_scheduler_type() const -> bool {
        return not view_.is_null(13);
    }

    auto scheduler_type() const -> std::string_view {
        auto v = view_.get_string(13);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_executor_threads() const -> bool {
        return not view_.is_null(14);
    }

    auto executor_threads() const -> int64_t {
        auto v = view_.get_int64(14);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_trigger() const -> bool {
        return not view_.is_null(15);
    }

    auto trigger_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(15);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_inputs() const -> bool {
        return not view_.is_null(16);
    }

    auto inputs_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_array_bytes(16);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_outputs() const -> bool {
        return not view_.is_null(17);
    }

    auto outputs_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_array_bytes(17);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_services() const -> bool {
        return not view_.is_null(18);
    }

    auto services_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_array_bytes(18);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ComponentLoadPlanBuilder {
    static constexpr size_t kFieldCount = 19;

    ComponentLoadPlanBuilder() : writer_(19) {}

    void set_module_name(std::string_view value) { writer_.set_string(0, value); }
    void set_component_name(std::string_view value) { writer_.set_string(1, value); }
    void set_component_group(std::string_view value) { writer_.set_string(2, value); }
    void set_component_class(std::string_view value) { writer_.set_string(3, value); }
    void set_component_library(std::string_view value) { writer_.set_string(4, value); }
    void set_static_link(bool value) { writer_.set_bool(5, value); }
    void set_priority(int64_t value) { writer_.set_int64(6, value); }
    void set_component_conf_path(std::string_view value) { writer_.set_string(7, value); }
    void set_module_conf_path(std::string_view value) { writer_.set_string(8, value); }
    void set_module_dir(std::string_view value) { writer_.set_string(9, value); }
    void set_module_config_dir(std::string_view value) { writer_.set_string(10, value); }
    void set_config_file_path(std::string_view value) { writer_.set_string(11, value); }
    void set_process_group(std::string_view value) { writer_.set_string(12, value); }
    void set_scheduler_type(std::string_view value) { writer_.set_string(13, value); }
    void set_executor_threads(int64_t value) { writer_.set_int64(14, value); }
    void set_trigger_bytes(std::span<const uint8_t> value) { writer_.set_object(15, value); }
    void set_inputs_bytes(std::span<const uint8_t> value) { writer_.set_array(16, value); }
    void set_outputs_bytes(std::span<const uint8_t> value) { writer_.set_array(17, value); }
    void set_services_bytes(std::span<const uint8_t> value) { writer_.set_array(18, value); }

    void clear_component_group() { writer_.set_null(2); }
    void clear_component_library() { writer_.set_null(4); }
    void clear_static_link() { writer_.set_null(5); }
    void clear_priority() { writer_.set_null(6); }
    void clear_component_conf_path() { writer_.set_null(7); }
    void clear_module_conf_path() { writer_.set_null(8); }
    void clear_module_dir() { writer_.set_null(9); }
    void clear_module_config_dir() { writer_.set_null(10); }
    void clear_config_file_path() { writer_.set_null(11); }
    void clear_process_group() { writer_.set_null(12); }
    void clear_scheduler_type() { writer_.set_null(13); }
    void clear_executor_threads() { writer_.set_null(14); }
    void clear_trigger() { writer_.set_null(15); }
    void clear_inputs() { writer_.set_null(16); }
    void clear_outputs() { writer_.set_null(17); }
    void clear_services() { writer_.set_null(18); }

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

inline auto ComponentLoadPlan::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ComponentLoadPlanBuilder();
    builder.set_module_name(module_name);
    builder.set_component_name(component_name);
    if (component_group.has_value()) {
        builder.set_component_group(*component_group);
    } else {
        builder.clear_component_group();
    }
    builder.set_component_class(component_class);
    if (component_library.has_value()) {
        builder.set_component_library(*component_library);
    } else {
        builder.clear_component_library();
    }
    if (static_link.has_value()) {
        builder.set_static_link(*static_link);
    } else {
        builder.clear_static_link();
    }
    if (priority.has_value()) {
        builder.set_priority(*priority);
    } else {
        builder.clear_priority();
    }
    if (component_conf_path.has_value()) {
        builder.set_component_conf_path(*component_conf_path);
    } else {
        builder.clear_component_conf_path();
    }
    if (module_conf_path.has_value()) {
        builder.set_module_conf_path(*module_conf_path);
    } else {
        builder.clear_module_conf_path();
    }
    if (module_dir.has_value()) {
        builder.set_module_dir(*module_dir);
    } else {
        builder.clear_module_dir();
    }
    if (module_config_dir.has_value()) {
        builder.set_module_config_dir(*module_config_dir);
    } else {
        builder.clear_module_config_dir();
    }
    if (config_file_path.has_value()) {
        builder.set_config_file_path(*config_file_path);
    } else {
        builder.clear_config_file_path();
    }
    if (process_group.has_value()) {
        builder.set_process_group(*process_group);
    } else {
        builder.clear_process_group();
    }
    if (scheduler_type.has_value()) {
        builder.set_scheduler_type(*scheduler_type);
    } else {
        builder.clear_scheduler_type();
    }
    if (executor_threads.has_value()) {
        builder.set_executor_threads(*executor_threads);
    } else {
        builder.clear_executor_threads();
    }
    if (trigger.has_value()) {
        { auto bytes = trigger->encode_fixed_layout(); builder.set_trigger_bytes(bytes); }
    } else {
        builder.clear_trigger();
    }
    if (inputs.has_value()) {
        { auto bytes = star::transport::encode_object_array(*inputs); builder.set_inputs_bytes(bytes); }
    } else {
        builder.clear_inputs();
    }
    if (outputs.has_value()) {
        { auto bytes = star::transport::encode_object_array(*outputs); builder.set_outputs_bytes(bytes); }
    } else {
        builder.clear_outputs();
    }
    if (services.has_value()) {
        { auto bytes = star::transport::encode_object_array(*services); builder.set_services_bytes(bytes); }
    } else {
        builder.clear_services();
    }
    return builder.Serialize();
}

inline auto ComponentLoadPlan::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ComponentLoadPlanBuilder();
    builder.set_module_name(module_name);
    builder.set_component_name(component_name);
    if (component_group.has_value()) {
        builder.set_component_group(*component_group);
    } else {
        builder.clear_component_group();
    }
    builder.set_component_class(component_class);
    if (component_library.has_value()) {
        builder.set_component_library(*component_library);
    } else {
        builder.clear_component_library();
    }
    if (static_link.has_value()) {
        builder.set_static_link(*static_link);
    } else {
        builder.clear_static_link();
    }
    if (priority.has_value()) {
        builder.set_priority(*priority);
    } else {
        builder.clear_priority();
    }
    if (component_conf_path.has_value()) {
        builder.set_component_conf_path(*component_conf_path);
    } else {
        builder.clear_component_conf_path();
    }
    if (module_conf_path.has_value()) {
        builder.set_module_conf_path(*module_conf_path);
    } else {
        builder.clear_module_conf_path();
    }
    if (module_dir.has_value()) {
        builder.set_module_dir(*module_dir);
    } else {
        builder.clear_module_dir();
    }
    if (module_config_dir.has_value()) {
        builder.set_module_config_dir(*module_config_dir);
    } else {
        builder.clear_module_config_dir();
    }
    if (config_file_path.has_value()) {
        builder.set_config_file_path(*config_file_path);
    } else {
        builder.clear_config_file_path();
    }
    if (process_group.has_value()) {
        builder.set_process_group(*process_group);
    } else {
        builder.clear_process_group();
    }
    if (scheduler_type.has_value()) {
        builder.set_scheduler_type(*scheduler_type);
    } else {
        builder.clear_scheduler_type();
    }
    if (executor_threads.has_value()) {
        builder.set_executor_threads(*executor_threads);
    } else {
        builder.clear_executor_threads();
    }
    if (trigger.has_value()) {
        { auto bytes = trigger->encode_fixed_layout(); builder.set_trigger_bytes(bytes); }
    } else {
        builder.clear_trigger();
    }
    if (inputs.has_value()) {
        { auto bytes = star::transport::encode_object_array(*inputs); builder.set_inputs_bytes(bytes); }
    } else {
        builder.clear_inputs();
    }
    if (outputs.has_value()) {
        { auto bytes = star::transport::encode_object_array(*outputs); builder.set_outputs_bytes(bytes); }
    } else {
        builder.clear_outputs();
    }
    if (services.has_value()) {
        { auto bytes = star::transport::encode_object_array(*services); builder.set_services_bytes(bytes); }
    } else {
        builder.clear_services();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ComponentLoadPlan::from_fixed_layout(std::span<const uint8_t> data) -> ComponentLoadPlan {
    auto view = ComponentLoadPlanView(data.data(), data.size());
    auto out = ComponentLoadPlan();
    out.module_name = view.module_name();
    out.component_name = view.component_name();
    if (view.Has_component_group()) { out.component_group = view.component_group(); }
    out.component_class = view.component_class();
    if (view.Has_component_library()) { out.component_library = view.component_library(); }
    if (view.Has_static_link()) { out.static_link = view.static_link(); }
    if (view.Has_priority()) { out.priority = view.priority(); }
    if (view.Has_component_conf_path()) { out.component_conf_path = view.component_conf_path(); }
    if (view.Has_module_conf_path()) { out.module_conf_path = view.module_conf_path(); }
    if (view.Has_module_dir()) { out.module_dir = view.module_dir(); }
    if (view.Has_module_config_dir()) { out.module_config_dir = view.module_config_dir(); }
    if (view.Has_config_file_path()) { out.config_file_path = view.config_file_path(); }
    if (view.Has_process_group()) { out.process_group = view.process_group(); }
    if (view.Has_scheduler_type()) { out.scheduler_type = view.scheduler_type(); }
    if (view.Has_executor_threads()) { out.executor_threads = view.executor_threads(); }
    if (view.Has_trigger()) { auto bytes = view.trigger_bytes(); out.trigger = TriggerPlan::from_fixed_layout(bytes); }
    if (view.Has_inputs()) { auto bytes = view.inputs_bytes(); out.inputs = star::transport::decode_object_array<PortPlan>(bytes); }
    if (view.Has_outputs()) { auto bytes = view.outputs_bytes(); out.outputs = star::transport::decode_object_array<PortPlan>(bytes); }
    if (view.Has_services()) { auto bytes = view.services_bytes(); out.services = star::transport::decode_object_array<ServicePortPlan>(bytes); }
    return out;
}

using ComponentLoadPlanSchema = ComponentLoadPlan;

}  // namespace star::generated::runtime::component_load_plan

#endif // __RUNTIME_COMPONENT_LOAD_PLAN__

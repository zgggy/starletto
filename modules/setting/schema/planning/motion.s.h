#ifndef __PLANNING_MOTION__
#define __PLANNING_MOTION__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::parameter::planning {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "planning.motion";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_Processor() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "expected_cycle_time";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "log_level";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_Methods() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "trajectory_method";
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
        field.name = "enable_fallback";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_MotionParameter() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "processor";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Processor";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_Processor();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "methods";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Methods";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_Methods();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.parameter.planning";
    out.root_name = "MotionParameter";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_MotionParameter();
    return out;
}

struct ProcessorBuilder;

struct Processor {
    static constexpr uint32_t kFieldId_expected_cycle_time = 1;
    static constexpr uint32_t kFieldId_log_level = 2;

    int64_t expected_cycle_time{};
    std::string log_level{};

    static auto from_value(const star::schema::SchemaValue& value) -> Processor {
        auto out = Processor{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("expected_cycle_time"); it != obj->end()) {
            out.expected_cycle_time = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: expected_cycle_time");
        }
        if (const auto it = obj->find("log_level"); it != obj->end()) {
            out.log_level = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: log_level");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("expected_cycle_time", schema_write(expected_cycle_time));
        obj.emplace("log_level", schema_write(log_level));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Processor;
};

struct ProcessorView {
    static constexpr size_t kFieldCount = 2;

    ProcessorView() = default;
    explicit ProcessorView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ProcessorView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_expected_cycle_time = 0;
    static constexpr size_t kSlotIndex_log_level = 1;

    auto Has_expected_cycle_time() const -> bool {
        return not view_.is_null(0);
    }

    auto expected_cycle_time() const -> int64_t {
        auto v = view_.get_int64(0);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_log_level() const -> bool {
        return not view_.is_null(1);
    }

    auto log_level() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ProcessorBuilder {
    static constexpr size_t kFieldCount = 2;

    ProcessorBuilder() : writer_(2) {}

    void set_expected_cycle_time(int64_t value) { writer_.set_int64(0, value); }
    void set_log_level(std::string_view value) { writer_.set_string(1, value); }

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

inline auto Processor::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ProcessorBuilder();
    builder.set_expected_cycle_time(expected_cycle_time);
    builder.set_log_level(log_level);
    return builder.Serialize();
}

inline auto Processor::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ProcessorBuilder();
    builder.set_expected_cycle_time(expected_cycle_time);
    builder.set_log_level(log_level);
    return builder.EncodeTo(buf, cap);
}

inline auto Processor::from_fixed_layout(std::span<const uint8_t> data) -> Processor {
    auto view = ProcessorView(data.data(), data.size());
    auto out = Processor();
    out.expected_cycle_time = view.expected_cycle_time();
    out.log_level = view.log_level();
    return out;
}

struct MethodsBuilder;

struct Methods {
    static constexpr uint32_t kFieldId_trajectory_method = 1;
    static constexpr uint32_t kFieldId_enable_fallback = 2;

    std::string trajectory_method{};
    bool enable_fallback{};

    static auto from_value(const star::schema::SchemaValue& value) -> Methods {
        auto out = Methods{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("trajectory_method"); it != obj->end()) {
            out.trajectory_method = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: trajectory_method");
        }
        if (const auto it = obj->find("enable_fallback"); it != obj->end()) {
            out.enable_fallback = schema_read<bool>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: enable_fallback");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("trajectory_method", schema_write(trajectory_method));
        obj.emplace("enable_fallback", schema_write(enable_fallback));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Methods;
};

struct MethodsView {
    static constexpr size_t kFieldCount = 2;

    MethodsView() = default;
    explicit MethodsView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit MethodsView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_trajectory_method = 0;
    static constexpr size_t kSlotIndex_enable_fallback = 1;

    auto Has_trajectory_method() const -> bool {
        return not view_.is_null(0);
    }

    auto trajectory_method() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_enable_fallback() const -> bool {
        return not view_.is_null(1);
    }

    auto enable_fallback() const -> bool {
        auto v = view_.get_bool(1);
        return v.has_value() ? *v : false;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct MethodsBuilder {
    static constexpr size_t kFieldCount = 2;

    MethodsBuilder() : writer_(2) {}

    void set_trajectory_method(std::string_view value) { writer_.set_string(0, value); }
    void set_enable_fallback(bool value) { writer_.set_bool(1, value); }

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

inline auto Methods::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = MethodsBuilder();
    builder.set_trajectory_method(trajectory_method);
    builder.set_enable_fallback(enable_fallback);
    return builder.Serialize();
}

inline auto Methods::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = MethodsBuilder();
    builder.set_trajectory_method(trajectory_method);
    builder.set_enable_fallback(enable_fallback);
    return builder.EncodeTo(buf, cap);
}

inline auto Methods::from_fixed_layout(std::span<const uint8_t> data) -> Methods {
    auto view = MethodsView(data.data(), data.size());
    auto out = Methods();
    out.trajectory_method = view.trajectory_method();
    out.enable_fallback = view.enable_fallback();
    return out;
}

struct MotionParameterBuilder;

struct MotionParameter {
    static constexpr uint32_t kFieldId_processor = 1;
    static constexpr uint32_t kFieldId_methods = 2;

    Processor processor{};
    std::optional<Methods> methods{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_methods() const -> bool {
        return methods.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> MotionParameter {
        auto out = MotionParameter{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("processor"); it != obj->end()) {
            out.processor = schema_read<Processor>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: processor");
        }
        if (const auto it = obj->find("methods"); it != obj->end()) {
            out.methods = schema_read<Methods>(it->second);
        } else {
            out.methods.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("processor", schema_write(processor));
        if (methods.has_value()) {
            obj.emplace("methods", schema_write(*methods));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> MotionParameter {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> MotionParameter;
};

struct MotionParameterView {
    static constexpr size_t kFieldCount = 2;

    MotionParameterView() = default;
    explicit MotionParameterView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit MotionParameterView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> MotionParameterView {
        return MotionParameterView(bytes);
    }

    static constexpr size_t kSlotIndex_processor = 0;
    static constexpr size_t kSlotIndex_methods = 1;

    auto Has_processor() const -> bool {
        return not view_.is_null(0);
    }

    auto processor_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_methods() const -> bool {
        return not view_.is_null(1);
    }

    auto methods_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(1);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct MotionParameterBuilder {
    static constexpr size_t kFieldCount = 2;

    MotionParameterBuilder() : writer_(2) {}

    void set_processor_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }
    void set_methods_bytes(std::span<const uint8_t> value) { writer_.set_object(1, value); }

    void clear_methods() { writer_.set_null(1); }

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

inline auto MotionParameter::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = MotionParameterBuilder();
    { auto bytes = processor.encode_fixed_layout(); builder.set_processor_bytes(bytes); }
    if (methods.has_value()) {
        { auto bytes = methods->encode_fixed_layout(); builder.set_methods_bytes(bytes); }
    } else {
        builder.clear_methods();
    }
    return builder.Serialize();
}

inline auto MotionParameter::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = MotionParameterBuilder();
    { auto bytes = processor.encode_fixed_layout(); builder.set_processor_bytes(bytes); }
    if (methods.has_value()) {
        { auto bytes = methods->encode_fixed_layout(); builder.set_methods_bytes(bytes); }
    } else {
        builder.clear_methods();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto MotionParameter::from_fixed_layout(std::span<const uint8_t> data) -> MotionParameter {
    auto view = MotionParameterView(data.data(), data.size());
    auto out = MotionParameter();
    { auto bytes = view.processor_bytes(); out.processor = Processor::from_fixed_layout(bytes); }
    if (view.Has_methods()) { auto bytes = view.methods_bytes(); out.methods = Methods::from_fixed_layout(bytes); }
    return out;
}

using MotionParameterSchema = MotionParameter;

}  // namespace star::setting::parameter::planning

#endif // __PLANNING_MOTION__

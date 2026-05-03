#ifndef __CONTROL_COMMAND__
#define __CONTROL_COMMAND__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::control {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "control.command";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ControlCommand() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "throttle";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "brake";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "steering";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "gear";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "frame_id";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int64";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "timestamp_ns";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int64";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.control";
    out.root_name = "ControlCommand";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ControlCommand();
    return out;
}

struct ControlCommandBuilder;

struct ControlCommand {
    static constexpr uint32_t kFieldId_throttle = 1;
    static constexpr uint32_t kFieldId_brake = 2;
    static constexpr uint32_t kFieldId_steering = 3;
    static constexpr uint32_t kFieldId_gear = 4;
    static constexpr uint32_t kFieldId_frame_id = 5;
    static constexpr uint32_t kFieldId_timestamp_ns = 6;

    double throttle{};
    double brake{};
    double steering{};
    int64_t gear{};
    int64_t frame_id{};
    int64_t timestamp_ns{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> ControlCommand {
        auto out = ControlCommand{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("throttle"); it != obj->end()) {
            out.throttle = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: throttle");
        }
        if (const auto it = obj->find("brake"); it != obj->end()) {
            out.brake = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: brake");
        }
        if (const auto it = obj->find("steering"); it != obj->end()) {
            out.steering = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: steering");
        }
        if (const auto it = obj->find("gear"); it != obj->end()) {
            out.gear = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: gear");
        }
        if (const auto it = obj->find("frame_id"); it != obj->end()) {
            out.frame_id = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: frame_id");
        }
        if (const auto it = obj->find("timestamp_ns"); it != obj->end()) {
            out.timestamp_ns = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: timestamp_ns");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("throttle", schema_write(throttle));
        obj.emplace("brake", schema_write(brake));
        obj.emplace("steering", schema_write(steering));
        obj.emplace("gear", schema_write(gear));
        obj.emplace("frame_id", schema_write(frame_id));
        obj.emplace("timestamp_ns", schema_write(timestamp_ns));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ControlCommand {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ControlCommand;
};

struct ControlCommandView {
    static constexpr size_t kFieldCount = 6;

    ControlCommandView() = default;
    explicit ControlCommandView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ControlCommandView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ControlCommandView {
        return ControlCommandView(bytes);
    }

    static constexpr size_t kSlotIndex_throttle = 0;
    static constexpr size_t kSlotIndex_brake = 1;
    static constexpr size_t kSlotIndex_steering = 2;
    static constexpr size_t kSlotIndex_gear = 3;
    static constexpr size_t kSlotIndex_frame_id = 4;
    static constexpr size_t kSlotIndex_timestamp_ns = 5;

    auto Has_throttle() const -> bool {
        return not view_.is_null(0);
    }

    auto throttle() const -> double {
        auto v = view_.get_float64(0);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_brake() const -> bool {
        return not view_.is_null(1);
    }

    auto brake() const -> double {
        auto v = view_.get_float64(1);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_steering() const -> bool {
        return not view_.is_null(2);
    }

    auto steering() const -> double {
        auto v = view_.get_float64(2);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_gear() const -> bool {
        return not view_.is_null(3);
    }

    auto gear() const -> int64_t {
        auto v = view_.get_int64(3);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_frame_id() const -> bool {
        return not view_.is_null(4);
    }

    auto frame_id() const -> int64_t {
        auto v = view_.get_int64(4);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_timestamp_ns() const -> bool {
        return not view_.is_null(5);
    }

    auto timestamp_ns() const -> int64_t {
        auto v = view_.get_int64(5);
        return v.has_value() ? *v : int64_t{0};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ControlCommandBuilder {
    static constexpr size_t kFieldCount = 6;

    ControlCommandBuilder() : writer_(6) {}

    void set_throttle(double value) { writer_.set_float64(0, value); }
    void set_brake(double value) { writer_.set_float64(1, value); }
    void set_steering(double value) { writer_.set_float64(2, value); }
    void set_gear(int64_t value) { writer_.set_int64(3, value); }
    void set_frame_id(int64_t value) { writer_.set_int64(4, value); }
    void set_timestamp_ns(int64_t value) { writer_.set_int64(5, value); }

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

inline auto ControlCommand::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ControlCommandBuilder();
    builder.set_throttle(throttle);
    builder.set_brake(brake);
    builder.set_steering(steering);
    builder.set_gear(gear);
    builder.set_frame_id(frame_id);
    builder.set_timestamp_ns(timestamp_ns);
    return builder.Serialize();
}

inline auto ControlCommand::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ControlCommandBuilder();
    builder.set_throttle(throttle);
    builder.set_brake(brake);
    builder.set_steering(steering);
    builder.set_gear(gear);
    builder.set_frame_id(frame_id);
    builder.set_timestamp_ns(timestamp_ns);
    return builder.EncodeTo(buf, cap);
}

inline auto ControlCommand::from_fixed_layout(std::span<const uint8_t> data) -> ControlCommand {
    auto view = ControlCommandView(data.data(), data.size());
    auto out = ControlCommand();
    out.throttle = view.throttle();
    out.brake = view.brake();
    out.steering = view.steering();
    out.gear = view.gear();
    out.frame_id = view.frame_id();
    out.timestamp_ns = view.timestamp_ns();
    return out;
}

using ControlCommandSchema = ControlCommand;

}  // namespace star::setting::control

#endif // __CONTROL_COMMAND__

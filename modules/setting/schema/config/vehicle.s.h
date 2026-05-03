#ifndef __VEHICLE_VEHICLE__
#define __VEHICLE_VEHICLE__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::config::vehicle {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "vehicle.vehicle";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_Vehicle() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "wheel_base_m";
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
        field.name = "max_steer_angle_deg";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_VehicleConfig() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "vehicle";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Vehicle";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_Vehicle();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.config.vehicle";
    out.root_name = "VehicleConfig";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_VehicleConfig();
    return out;
}

struct VehicleBuilder;

struct Vehicle {
    static constexpr uint32_t kFieldId_wheel_base_m = 1;
    static constexpr uint32_t kFieldId_max_steer_angle_deg = 2;

    double wheel_base_m{};
    double max_steer_angle_deg{};

    static auto from_value(const star::schema::SchemaValue& value) -> Vehicle {
        auto out = Vehicle{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("wheel_base_m"); it != obj->end()) {
            out.wheel_base_m = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: wheel_base_m");
        }
        if (const auto it = obj->find("max_steer_angle_deg"); it != obj->end()) {
            out.max_steer_angle_deg = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: max_steer_angle_deg");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("wheel_base_m", schema_write(wheel_base_m));
        obj.emplace("max_steer_angle_deg", schema_write(max_steer_angle_deg));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Vehicle;
};

struct VehicleView {
    static constexpr size_t kFieldCount = 2;

    VehicleView() = default;
    explicit VehicleView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit VehicleView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_wheel_base_m = 0;
    static constexpr size_t kSlotIndex_max_steer_angle_deg = 1;

    auto Has_wheel_base_m() const -> bool {
        return not view_.is_null(0);
    }

    auto wheel_base_m() const -> double {
        auto v = view_.get_float64(0);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_max_steer_angle_deg() const -> bool {
        return not view_.is_null(1);
    }

    auto max_steer_angle_deg() const -> double {
        auto v = view_.get_float64(1);
        return v.has_value() ? *v : 0.0;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct VehicleBuilder {
    static constexpr size_t kFieldCount = 2;

    VehicleBuilder() : writer_(2) {}

    void set_wheel_base_m(double value) { writer_.set_float64(0, value); }
    void set_max_steer_angle_deg(double value) { writer_.set_float64(1, value); }

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

inline auto Vehicle::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = VehicleBuilder();
    builder.set_wheel_base_m(wheel_base_m);
    builder.set_max_steer_angle_deg(max_steer_angle_deg);
    return builder.Serialize();
}

inline auto Vehicle::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = VehicleBuilder();
    builder.set_wheel_base_m(wheel_base_m);
    builder.set_max_steer_angle_deg(max_steer_angle_deg);
    return builder.EncodeTo(buf, cap);
}

inline auto Vehicle::from_fixed_layout(std::span<const uint8_t> data) -> Vehicle {
    auto view = VehicleView(data.data(), data.size());
    auto out = Vehicle();
    out.wheel_base_m = view.wheel_base_m();
    out.max_steer_angle_deg = view.max_steer_angle_deg();
    return out;
}

struct VehicleConfigBuilder;

struct VehicleConfig {
    static constexpr uint32_t kFieldId_vehicle = 1;

    Vehicle vehicle{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> VehicleConfig {
        auto out = VehicleConfig{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("vehicle"); it != obj->end()) {
            out.vehicle = schema_read<Vehicle>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: vehicle");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("vehicle", schema_write(vehicle));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> VehicleConfig {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> VehicleConfig;
};

struct VehicleConfigView {
    static constexpr size_t kFieldCount = 1;

    VehicleConfigView() = default;
    explicit VehicleConfigView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit VehicleConfigView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> VehicleConfigView {
        return VehicleConfigView(bytes);
    }

    static constexpr size_t kSlotIndex_vehicle = 0;

    auto Has_vehicle() const -> bool {
        return not view_.is_null(0);
    }

    auto vehicle_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct VehicleConfigBuilder {
    static constexpr size_t kFieldCount = 1;

    VehicleConfigBuilder() : writer_(1) {}

    void set_vehicle_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }

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

inline auto VehicleConfig::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = VehicleConfigBuilder();
    { auto bytes = vehicle.encode_fixed_layout(); builder.set_vehicle_bytes(bytes); }
    return builder.Serialize();
}

inline auto VehicleConfig::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = VehicleConfigBuilder();
    { auto bytes = vehicle.encode_fixed_layout(); builder.set_vehicle_bytes(bytes); }
    return builder.EncodeTo(buf, cap);
}

inline auto VehicleConfig::from_fixed_layout(std::span<const uint8_t> data) -> VehicleConfig {
    auto view = VehicleConfigView(data.data(), data.size());
    auto out = VehicleConfig();
    { auto bytes = view.vehicle_bytes(); out.vehicle = Vehicle::from_fixed_layout(bytes); }
    return out;
}

using VehicleConfigSchema = VehicleConfig;

}  // namespace star::setting::config::vehicle

#endif // __VEHICLE_VEHICLE__

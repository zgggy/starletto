#ifndef __SENSOR_SENSOR__
#define __SENSOR_SENSOR__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::config::sensor {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "sensor.sensor";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_Sensor() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "primary_lidar";
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
        field.name = "enable_camera_fusion";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_SensorConfig() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "sensor";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Sensor";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_Sensor();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.config.sensor";
    out.root_name = "SensorConfig";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_SensorConfig();
    return out;
}

struct SensorBuilder;

struct Sensor {
    static constexpr uint32_t kFieldId_primary_lidar = 1;
    static constexpr uint32_t kFieldId_enable_camera_fusion = 2;

    std::string primary_lidar{};
    bool enable_camera_fusion{};

    static auto from_value(const star::schema::SchemaValue& value) -> Sensor {
        auto out = Sensor{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("primary_lidar"); it != obj->end()) {
            out.primary_lidar = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: primary_lidar");
        }
        if (const auto it = obj->find("enable_camera_fusion"); it != obj->end()) {
            out.enable_camera_fusion = schema_read<bool>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: enable_camera_fusion");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("primary_lidar", schema_write(primary_lidar));
        obj.emplace("enable_camera_fusion", schema_write(enable_camera_fusion));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Sensor;
};

struct SensorView {
    static constexpr size_t kFieldCount = 2;

    SensorView() = default;
    explicit SensorView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit SensorView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_primary_lidar = 0;
    static constexpr size_t kSlotIndex_enable_camera_fusion = 1;

    auto Has_primary_lidar() const -> bool {
        return not view_.is_null(0);
    }

    auto primary_lidar() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_enable_camera_fusion() const -> bool {
        return not view_.is_null(1);
    }

    auto enable_camera_fusion() const -> bool {
        auto v = view_.get_bool(1);
        return v.has_value() ? *v : false;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct SensorBuilder {
    static constexpr size_t kFieldCount = 2;

    SensorBuilder() : writer_(2) {}

    void set_primary_lidar(std::string_view value) { writer_.set_string(0, value); }
    void set_enable_camera_fusion(bool value) { writer_.set_bool(1, value); }

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

inline auto Sensor::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = SensorBuilder();
    builder.set_primary_lidar(primary_lidar);
    builder.set_enable_camera_fusion(enable_camera_fusion);
    return builder.Serialize();
}

inline auto Sensor::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = SensorBuilder();
    builder.set_primary_lidar(primary_lidar);
    builder.set_enable_camera_fusion(enable_camera_fusion);
    return builder.EncodeTo(buf, cap);
}

inline auto Sensor::from_fixed_layout(std::span<const uint8_t> data) -> Sensor {
    auto view = SensorView(data.data(), data.size());
    auto out = Sensor();
    out.primary_lidar = view.primary_lidar();
    out.enable_camera_fusion = view.enable_camera_fusion();
    return out;
}

struct SensorConfigBuilder;

struct SensorConfig {
    static constexpr uint32_t kFieldId_sensor = 1;

    Sensor sensor{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> SensorConfig {
        auto out = SensorConfig{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("sensor"); it != obj->end()) {
            out.sensor = schema_read<Sensor>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: sensor");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("sensor", schema_write(sensor));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> SensorConfig {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> SensorConfig;
};

struct SensorConfigView {
    static constexpr size_t kFieldCount = 1;

    SensorConfigView() = default;
    explicit SensorConfigView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit SensorConfigView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> SensorConfigView {
        return SensorConfigView(bytes);
    }

    static constexpr size_t kSlotIndex_sensor = 0;

    auto Has_sensor() const -> bool {
        return not view_.is_null(0);
    }

    auto sensor_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct SensorConfigBuilder {
    static constexpr size_t kFieldCount = 1;

    SensorConfigBuilder() : writer_(1) {}

    void set_sensor_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }

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

inline auto SensorConfig::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = SensorConfigBuilder();
    { auto bytes = sensor.encode_fixed_layout(); builder.set_sensor_bytes(bytes); }
    return builder.Serialize();
}

inline auto SensorConfig::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = SensorConfigBuilder();
    { auto bytes = sensor.encode_fixed_layout(); builder.set_sensor_bytes(bytes); }
    return builder.EncodeTo(buf, cap);
}

inline auto SensorConfig::from_fixed_layout(std::span<const uint8_t> data) -> SensorConfig {
    auto view = SensorConfigView(data.data(), data.size());
    auto out = SensorConfig();
    { auto bytes = view.sensor_bytes(); out.sensor = Sensor::from_fixed_layout(bytes); }
    return out;
}

using SensorConfigSchema = SensorConfig;

}  // namespace star::setting::config::sensor

#endif // __SENSOR_SENSOR__

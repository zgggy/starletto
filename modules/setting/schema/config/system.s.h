#ifndef __SYSTEM_SYSTEM__
#define __SYSTEM_SYSTEM__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::config::system {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "system.system";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_System() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "clock_mode";
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
        field.name = "enable_monitor";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_SystemConfig() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "system";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "System";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_System();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.config.system";
    out.root_name = "SystemConfig";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_SystemConfig();
    return out;
}

struct SystemBuilder;

struct System {
    static constexpr uint32_t kFieldId_clock_mode = 1;
    static constexpr uint32_t kFieldId_enable_monitor = 2;

    std::string clock_mode{};
    bool enable_monitor{};

    static auto from_value(const star::schema::SchemaValue& value) -> System {
        auto out = System{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("clock_mode"); it != obj->end()) {
            out.clock_mode = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: clock_mode");
        }
        if (const auto it = obj->find("enable_monitor"); it != obj->end()) {
            out.enable_monitor = schema_read<bool>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: enable_monitor");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("clock_mode", schema_write(clock_mode));
        obj.emplace("enable_monitor", schema_write(enable_monitor));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> System;
};

struct SystemView {
    static constexpr size_t kFieldCount = 2;

    SystemView() = default;
    explicit SystemView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit SystemView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_clock_mode = 0;
    static constexpr size_t kSlotIndex_enable_monitor = 1;

    auto Has_clock_mode() const -> bool {
        return not view_.is_null(0);
    }

    auto clock_mode() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_enable_monitor() const -> bool {
        return not view_.is_null(1);
    }

    auto enable_monitor() const -> bool {
        auto v = view_.get_bool(1);
        return v.has_value() ? *v : false;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct SystemBuilder {
    static constexpr size_t kFieldCount = 2;

    SystemBuilder() : writer_(2) {}

    void set_clock_mode(std::string_view value) { writer_.set_string(0, value); }
    void set_enable_monitor(bool value) { writer_.set_bool(1, value); }

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

inline auto System::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = SystemBuilder();
    builder.set_clock_mode(clock_mode);
    builder.set_enable_monitor(enable_monitor);
    return builder.Serialize();
}

inline auto System::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = SystemBuilder();
    builder.set_clock_mode(clock_mode);
    builder.set_enable_monitor(enable_monitor);
    return builder.EncodeTo(buf, cap);
}

inline auto System::from_fixed_layout(std::span<const uint8_t> data) -> System {
    auto view = SystemView(data.data(), data.size());
    auto out = System();
    out.clock_mode = view.clock_mode();
    out.enable_monitor = view.enable_monitor();
    return out;
}

struct SystemConfigBuilder;

struct SystemConfig {
    static constexpr uint32_t kFieldId_system = 1;

    System system{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> SystemConfig {
        auto out = SystemConfig{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("system"); it != obj->end()) {
            out.system = schema_read<System>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: system");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("system", schema_write(system));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> SystemConfig {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> SystemConfig;
};

struct SystemConfigView {
    static constexpr size_t kFieldCount = 1;

    SystemConfigView() = default;
    explicit SystemConfigView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit SystemConfigView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> SystemConfigView {
        return SystemConfigView(bytes);
    }

    static constexpr size_t kSlotIndex_system = 0;

    auto Has_system() const -> bool {
        return not view_.is_null(0);
    }

    auto system_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct SystemConfigBuilder {
    static constexpr size_t kFieldCount = 1;

    SystemConfigBuilder() : writer_(1) {}

    void set_system_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }

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

inline auto SystemConfig::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = SystemConfigBuilder();
    { auto bytes = system.encode_fixed_layout(); builder.set_system_bytes(bytes); }
    return builder.Serialize();
}

inline auto SystemConfig::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = SystemConfigBuilder();
    { auto bytes = system.encode_fixed_layout(); builder.set_system_bytes(bytes); }
    return builder.EncodeTo(buf, cap);
}

inline auto SystemConfig::from_fixed_layout(std::span<const uint8_t> data) -> SystemConfig {
    auto view = SystemConfigView(data.data(), data.size());
    auto out = SystemConfig();
    { auto bytes = view.system_bytes(); out.system = System::from_fixed_layout(bytes); }
    return out;
}

using SystemConfigSchema = SystemConfig;

}  // namespace star::setting::config::system

#endif // __SYSTEM_SYSTEM__

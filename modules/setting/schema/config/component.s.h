#ifndef __COMPONENT_COMPONENT__
#define __COMPONENT_COMPONENT__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::config::component {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "component.component";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_Component() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "default_executor_threads";
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
        field.name = "enable_runtime_stats";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_ComponentConfig() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "component";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Component";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_Component();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.config.component";
    out.root_name = "ComponentConfig";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ComponentConfig();
    return out;
}

struct ComponentBuilder;

struct Component {
    static constexpr uint32_t kFieldId_default_executor_threads = 1;
    static constexpr uint32_t kFieldId_enable_runtime_stats = 2;

    int64_t default_executor_threads{};
    bool enable_runtime_stats{};

    static auto from_value(const star::schema::SchemaValue& value) -> Component {
        auto out = Component{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("default_executor_threads"); it != obj->end()) {
            out.default_executor_threads = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: default_executor_threads");
        }
        if (const auto it = obj->find("enable_runtime_stats"); it != obj->end()) {
            out.enable_runtime_stats = schema_read<bool>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: enable_runtime_stats");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("default_executor_threads", schema_write(default_executor_threads));
        obj.emplace("enable_runtime_stats", schema_write(enable_runtime_stats));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Component;
};

struct ComponentView {
    static constexpr size_t kFieldCount = 2;

    ComponentView() = default;
    explicit ComponentView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ComponentView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_default_executor_threads = 0;
    static constexpr size_t kSlotIndex_enable_runtime_stats = 1;

    auto Has_default_executor_threads() const -> bool {
        return not view_.is_null(0);
    }

    auto default_executor_threads() const -> int64_t {
        auto v = view_.get_int64(0);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_enable_runtime_stats() const -> bool {
        return not view_.is_null(1);
    }

    auto enable_runtime_stats() const -> bool {
        auto v = view_.get_bool(1);
        return v.has_value() ? *v : false;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ComponentBuilder {
    static constexpr size_t kFieldCount = 2;

    ComponentBuilder() : writer_(2) {}

    void set_default_executor_threads(int64_t value) { writer_.set_int64(0, value); }
    void set_enable_runtime_stats(bool value) { writer_.set_bool(1, value); }

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

inline auto Component::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ComponentBuilder();
    builder.set_default_executor_threads(default_executor_threads);
    builder.set_enable_runtime_stats(enable_runtime_stats);
    return builder.Serialize();
}

inline auto Component::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ComponentBuilder();
    builder.set_default_executor_threads(default_executor_threads);
    builder.set_enable_runtime_stats(enable_runtime_stats);
    return builder.EncodeTo(buf, cap);
}

inline auto Component::from_fixed_layout(std::span<const uint8_t> data) -> Component {
    auto view = ComponentView(data.data(), data.size());
    auto out = Component();
    out.default_executor_threads = view.default_executor_threads();
    out.enable_runtime_stats = view.enable_runtime_stats();
    return out;
}

struct ComponentConfigBuilder;

struct ComponentConfig {
    static constexpr uint32_t kFieldId_component = 1;

    Component component{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> ComponentConfig {
        auto out = ComponentConfig{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("component"); it != obj->end()) {
            out.component = schema_read<Component>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: component");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("component", schema_write(component));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ComponentConfig {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ComponentConfig;
};

struct ComponentConfigView {
    static constexpr size_t kFieldCount = 1;

    ComponentConfigView() = default;
    explicit ComponentConfigView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ComponentConfigView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ComponentConfigView {
        return ComponentConfigView(bytes);
    }

    static constexpr size_t kSlotIndex_component = 0;

    auto Has_component() const -> bool {
        return not view_.is_null(0);
    }

    auto component_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ComponentConfigBuilder {
    static constexpr size_t kFieldCount = 1;

    ComponentConfigBuilder() : writer_(1) {}

    void set_component_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }

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

inline auto ComponentConfig::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ComponentConfigBuilder();
    { auto bytes = component.encode_fixed_layout(); builder.set_component_bytes(bytes); }
    return builder.Serialize();
}

inline auto ComponentConfig::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ComponentConfigBuilder();
    { auto bytes = component.encode_fixed_layout(); builder.set_component_bytes(bytes); }
    return builder.EncodeTo(buf, cap);
}

inline auto ComponentConfig::from_fixed_layout(std::span<const uint8_t> data) -> ComponentConfig {
    auto view = ComponentConfigView(data.data(), data.size());
    auto out = ComponentConfig();
    { auto bytes = view.component_bytes(); out.component = Component::from_fixed_layout(bytes); }
    return out;
}

using ComponentConfigSchema = ComponentConfig;

}  // namespace star::setting::config::component

#endif // __COMPONENT_COMPONENT__

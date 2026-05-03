#ifndef __SETTING_PARAMETER_SERVICE_SET_REQUEST__
#define __SETTING_PARAMETER_SERVICE_SET_REQUEST__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::service::parameter::set_request {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "setting.parameter.service.set_request";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ParameterSetRequest() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "key";
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
        field.name = "schema_name";
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
        field.name = "payload_binary";
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
        field.name = "debug_text";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.service.parameter.set_request";
    out.root_name = "ParameterSetRequest";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ParameterSetRequest();
    return out;
}

struct ParameterSetRequestBuilder;

struct ParameterSetRequest {
    static constexpr uint32_t kFieldId_key = 1;
    static constexpr uint32_t kFieldId_schema_name = 2;
    static constexpr uint32_t kFieldId_payload_binary = 3;
    static constexpr uint32_t kFieldId_debug_text = 4;

    std::string key{};
    std::optional<std::string> schema_name{std::nullopt};
    std::optional<std::string> payload_binary{std::nullopt};
    std::optional<std::string> debug_text{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_schema_name() const -> bool {
        return schema_name.has_value();
    }

    auto has_payload_binary() const -> bool {
        return payload_binary.has_value();
    }

    auto has_debug_text() const -> bool {
        return debug_text.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ParameterSetRequest {
        auto out = ParameterSetRequest{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("key"); it != obj->end()) {
            out.key = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: key");
        }
        if (const auto it = obj->find("schema_name"); it != obj->end()) {
            out.schema_name = schema_read<std::string>(it->second);
        } else {
            out.schema_name.reset();
        }
        if (const auto it = obj->find("payload_binary"); it != obj->end()) {
            out.payload_binary = schema_read<std::string>(it->second);
        } else {
            out.payload_binary.reset();
        }
        if (const auto it = obj->find("debug_text"); it != obj->end()) {
            out.debug_text = schema_read<std::string>(it->second);
        } else {
            out.debug_text.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("key", schema_write(key));
        if (schema_name.has_value()) {
            obj.emplace("schema_name", schema_write(*schema_name));
        }
        if (payload_binary.has_value()) {
            obj.emplace("payload_binary", schema_write(*payload_binary));
        }
        if (debug_text.has_value()) {
            obj.emplace("debug_text", schema_write(*debug_text));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ParameterSetRequest {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ParameterSetRequest;
};

struct ParameterSetRequestView {
    static constexpr size_t kFieldCount = 4;

    ParameterSetRequestView() = default;
    explicit ParameterSetRequestView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ParameterSetRequestView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ParameterSetRequestView {
        return ParameterSetRequestView(bytes);
    }

    static constexpr size_t kSlotIndex_key = 0;
    static constexpr size_t kSlotIndex_schema_name = 1;
    static constexpr size_t kSlotIndex_payload_binary = 2;
    static constexpr size_t kSlotIndex_debug_text = 3;

    auto Has_key() const -> bool {
        return not view_.is_null(0);
    }

    auto key() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_schema_name() const -> bool {
        return not view_.is_null(1);
    }

    auto schema_name() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_payload_binary() const -> bool {
        return not view_.is_null(2);
    }

    auto payload_binary() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_debug_text() const -> bool {
        return not view_.is_null(3);
    }

    auto debug_text() const -> std::string_view {
        auto v = view_.get_string(3);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ParameterSetRequestBuilder {
    static constexpr size_t kFieldCount = 4;

    ParameterSetRequestBuilder() : writer_(4) {}

    void set_key(std::string_view value) { writer_.set_string(0, value); }
    void set_schema_name(std::string_view value) { writer_.set_string(1, value); }
    void set_payload_binary(std::string_view value) { writer_.set_string(2, value); }
    void set_debug_text(std::string_view value) { writer_.set_string(3, value); }

    void clear_schema_name() { writer_.set_null(1); }
    void clear_payload_binary() { writer_.set_null(2); }
    void clear_debug_text() { writer_.set_null(3); }

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

inline auto ParameterSetRequest::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ParameterSetRequestBuilder();
    builder.set_key(key);
    if (schema_name.has_value()) {
        builder.set_schema_name(*schema_name);
    } else {
        builder.clear_schema_name();
    }
    if (payload_binary.has_value()) {
        builder.set_payload_binary(*payload_binary);
    } else {
        builder.clear_payload_binary();
    }
    if (debug_text.has_value()) {
        builder.set_debug_text(*debug_text);
    } else {
        builder.clear_debug_text();
    }
    return builder.Serialize();
}

inline auto ParameterSetRequest::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ParameterSetRequestBuilder();
    builder.set_key(key);
    if (schema_name.has_value()) {
        builder.set_schema_name(*schema_name);
    } else {
        builder.clear_schema_name();
    }
    if (payload_binary.has_value()) {
        builder.set_payload_binary(*payload_binary);
    } else {
        builder.clear_payload_binary();
    }
    if (debug_text.has_value()) {
        builder.set_debug_text(*debug_text);
    } else {
        builder.clear_debug_text();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ParameterSetRequest::from_fixed_layout(std::span<const uint8_t> data) -> ParameterSetRequest {
    auto view = ParameterSetRequestView(data.data(), data.size());
    auto out = ParameterSetRequest();
    out.key = view.key();
    if (view.Has_schema_name()) { out.schema_name = view.schema_name(); }
    if (view.Has_payload_binary()) { out.payload_binary = view.payload_binary(); }
    if (view.Has_debug_text()) { out.debug_text = view.debug_text(); }
    return out;
}

using ParameterSetRequestSchema = ParameterSetRequest;

}  // namespace star::setting::service::parameter::set_request

#endif // __SETTING_PARAMETER_SERVICE_SET_REQUEST__

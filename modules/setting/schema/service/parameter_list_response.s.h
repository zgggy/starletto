#ifndef __SETTING_PARAMETER_SERVICE_LIST_RESPONSE__
#define __SETTING_PARAMETER_SERVICE_LIST_RESPONSE__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::service::parameter::list_response {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "setting.parameter.service.list_response";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ParameterListResponse() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "keys";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = true;
        field.explicit_required = false;
        field.repeated = true;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.service.parameter.list_response";
    out.root_name = "ParameterListResponse";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ParameterListResponse();
    return out;
}

struct ParameterListResponseBuilder;

struct ParameterListResponse {
    static constexpr uint32_t kFieldId_keys = 1;

    std::optional<std::vector<std::string>> keys{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_keys() const -> bool {
        return keys.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ParameterListResponse {
        auto out = ParameterListResponse{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("keys"); it != obj->end()) {
            out.keys = schema_read<std::vector<std::string>>(it->second);
        } else {
            out.keys.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (keys.has_value()) {
            obj.emplace("keys", schema_write(*keys));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ParameterListResponse {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ParameterListResponse;
};

struct ParameterListResponseView {
    static constexpr size_t kFieldCount = 1;

    ParameterListResponseView() = default;
    explicit ParameterListResponseView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ParameterListResponseView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ParameterListResponseView {
        return ParameterListResponseView(bytes);
    }

    static constexpr size_t kSlotIndex_keys = 0;

    auto Has_keys() const -> bool {
        return not view_.is_null(0);
    }

    auto keys() const -> std::vector<std::string> {
        auto v = view_.get_array_bytes(0);
        return v.has_value() ? star::transport::decode_string_array(*v) : std::vector<std::string>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ParameterListResponseBuilder {
    static constexpr size_t kFieldCount = 1;

    ParameterListResponseBuilder() : writer_(1) {}

    void set_keys(const std::vector<std::string>& value) { auto bytes = star::transport::encode_string_array(value); writer_.set_array(0, bytes); }

    void clear_keys() { writer_.set_null(0); }

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

inline auto ParameterListResponse::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ParameterListResponseBuilder();
    if (keys.has_value()) {
        builder.set_keys(*keys);
    } else {
        builder.clear_keys();
    }
    return builder.Serialize();
}

inline auto ParameterListResponse::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ParameterListResponseBuilder();
    if (keys.has_value()) {
        builder.set_keys(*keys);
    } else {
        builder.clear_keys();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ParameterListResponse::from_fixed_layout(std::span<const uint8_t> data) -> ParameterListResponse {
    auto view = ParameterListResponseView(data.data(), data.size());
    auto out = ParameterListResponse();
    if (view.Has_keys()) { out.keys = view.keys(); }
    return out;
}

using ParameterListResponseSchema = ParameterListResponse;

}  // namespace star::setting::service::parameter::list_response

#endif // __SETTING_PARAMETER_SERVICE_LIST_RESPONSE__

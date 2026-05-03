#ifndef __SETTING_PARAMETER_SERVICE_GET_REQUEST__
#define __SETTING_PARAMETER_SERVICE_GET_REQUEST__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::service::parameter::get_request {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "setting.parameter.service.get_request";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ParameterGetRequest() -> std::vector<star::schema::FieldDescriptor> {
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
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.service.parameter.get_request";
    out.root_name = "ParameterGetRequest";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ParameterGetRequest();
    return out;
}

struct ParameterGetRequestBuilder;

struct ParameterGetRequest {
    static constexpr uint32_t kFieldId_key = 1;

    std::string key{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> ParameterGetRequest {
        auto out = ParameterGetRequest{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("key"); it != obj->end()) {
            out.key = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: key");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("key", schema_write(key));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ParameterGetRequest {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ParameterGetRequest;
};

struct ParameterGetRequestView {
    static constexpr size_t kFieldCount = 1;

    ParameterGetRequestView() = default;
    explicit ParameterGetRequestView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ParameterGetRequestView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ParameterGetRequestView {
        return ParameterGetRequestView(bytes);
    }

    static constexpr size_t kSlotIndex_key = 0;

    auto Has_key() const -> bool {
        return not view_.is_null(0);
    }

    auto key() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ParameterGetRequestBuilder {
    static constexpr size_t kFieldCount = 1;

    ParameterGetRequestBuilder() : writer_(1) {}

    void set_key(std::string_view value) { writer_.set_string(0, value); }

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

inline auto ParameterGetRequest::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ParameterGetRequestBuilder();
    builder.set_key(key);
    return builder.Serialize();
}

inline auto ParameterGetRequest::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ParameterGetRequestBuilder();
    builder.set_key(key);
    return builder.EncodeTo(buf, cap);
}

inline auto ParameterGetRequest::from_fixed_layout(std::span<const uint8_t> data) -> ParameterGetRequest {
    auto view = ParameterGetRequestView(data.data(), data.size());
    auto out = ParameterGetRequest();
    out.key = view.key();
    return out;
}

using ParameterGetRequestSchema = ParameterGetRequest;

}  // namespace star::setting::service::parameter::get_request

#endif // __SETTING_PARAMETER_SERVICE_GET_REQUEST__

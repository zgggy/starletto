#ifndef __SETTING_PARAMETER_SERVICE_LIST_REQUEST__
#define __SETTING_PARAMETER_SERVICE_LIST_REQUEST__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::service::parameter::list_request {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "setting.parameter.service.list_request";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ParameterListRequest() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "prefix";
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
    out.namespace_name = "star.setting.service.parameter.list_request";
    out.root_name = "ParameterListRequest";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ParameterListRequest();
    return out;
}

struct ParameterListRequestBuilder;

struct ParameterListRequest {
    static constexpr uint32_t kFieldId_prefix = 1;

    std::optional<std::string> prefix{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_prefix() const -> bool {
        return prefix.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> ParameterListRequest {
        auto out = ParameterListRequest{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("prefix"); it != obj->end()) {
            out.prefix = schema_read<std::string>(it->second);
        } else {
            out.prefix.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        if (prefix.has_value()) {
            obj.emplace("prefix", schema_write(*prefix));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ParameterListRequest {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ParameterListRequest;
};

struct ParameterListRequestView {
    static constexpr size_t kFieldCount = 1;

    ParameterListRequestView() = default;
    explicit ParameterListRequestView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ParameterListRequestView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ParameterListRequestView {
        return ParameterListRequestView(bytes);
    }

    static constexpr size_t kSlotIndex_prefix = 0;

    auto Has_prefix() const -> bool {
        return not view_.is_null(0);
    }

    auto prefix() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ParameterListRequestBuilder {
    static constexpr size_t kFieldCount = 1;

    ParameterListRequestBuilder() : writer_(1) {}

    void set_prefix(std::string_view value) { writer_.set_string(0, value); }

    void clear_prefix() { writer_.set_null(0); }

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

inline auto ParameterListRequest::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ParameterListRequestBuilder();
    if (prefix.has_value()) {
        builder.set_prefix(*prefix);
    } else {
        builder.clear_prefix();
    }
    return builder.Serialize();
}

inline auto ParameterListRequest::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ParameterListRequestBuilder();
    if (prefix.has_value()) {
        builder.set_prefix(*prefix);
    } else {
        builder.clear_prefix();
    }
    return builder.EncodeTo(buf, cap);
}

inline auto ParameterListRequest::from_fixed_layout(std::span<const uint8_t> data) -> ParameterListRequest {
    auto view = ParameterListRequestView(data.data(), data.size());
    auto out = ParameterListRequest();
    if (view.Has_prefix()) { out.prefix = view.prefix(); }
    return out;
}

using ParameterListRequestSchema = ParameterListRequest;

}  // namespace star::setting::service::parameter::list_request

#endif // __SETTING_PARAMETER_SERVICE_LIST_REQUEST__

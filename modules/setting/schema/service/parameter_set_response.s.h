#ifndef __SETTING_PARAMETER_SERVICE_SET_RESPONSE__
#define __SETTING_PARAMETER_SERVICE_SET_RESPONSE__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::service::parameter::set_response {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "setting.parameter.service.set_response";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ParameterSetResponse() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "updated";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
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
    out.namespace_name = "star.setting.service.parameter.set_response";
    out.root_name = "ParameterSetResponse";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ParameterSetResponse();
    return out;
}

struct ParameterSetResponseBuilder;

struct ParameterSetResponse {
    static constexpr uint32_t kFieldId_updated = 1;

    bool updated{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> ParameterSetResponse {
        auto out = ParameterSetResponse{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("updated"); it != obj->end()) {
            out.updated = schema_read<bool>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: updated");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("updated", schema_write(updated));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ParameterSetResponse {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ParameterSetResponse;
};

struct ParameterSetResponseView {
    static constexpr size_t kFieldCount = 1;

    ParameterSetResponseView() = default;
    explicit ParameterSetResponseView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ParameterSetResponseView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ParameterSetResponseView {
        return ParameterSetResponseView(bytes);
    }

    static constexpr size_t kSlotIndex_updated = 0;

    auto Has_updated() const -> bool {
        return not view_.is_null(0);
    }

    auto updated() const -> bool {
        auto v = view_.get_bool(0);
        return v.has_value() ? *v : false;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ParameterSetResponseBuilder {
    static constexpr size_t kFieldCount = 1;

    ParameterSetResponseBuilder() : writer_(1) {}

    void set_updated(bool value) { writer_.set_bool(0, value); }

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

inline auto ParameterSetResponse::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ParameterSetResponseBuilder();
    builder.set_updated(updated);
    return builder.Serialize();
}

inline auto ParameterSetResponse::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ParameterSetResponseBuilder();
    builder.set_updated(updated);
    return builder.EncodeTo(buf, cap);
}

inline auto ParameterSetResponse::from_fixed_layout(std::span<const uint8_t> data) -> ParameterSetResponse {
    auto view = ParameterSetResponseView(data.data(), data.size());
    auto out = ParameterSetResponse();
    out.updated = view.updated();
    return out;
}

using ParameterSetResponseSchema = ParameterSetResponse;

}  // namespace star::setting::service::parameter::set_response

#endif // __SETTING_PARAMETER_SERVICE_SET_RESPONSE__

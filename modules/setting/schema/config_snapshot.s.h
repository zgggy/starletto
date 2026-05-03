#ifndef __STAR_SETTING_CONFIG_SNAPSHOT__
#define __STAR_SETTING_CONFIG_SNAPSHOT__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::generated::setting::config::snapshot {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "star.setting.config.snapshot";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_SnapshotEntry() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "schema_name";
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
        field.id = 3;
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

inline auto _build_fields_ConfigSnapshot() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "entries";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "SnapshotEntry";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.has_value_kind = true;
        field.value_kind = star::schema::FieldKind::OBJECT;
        field.value_type_name = "SnapshotEntry";
        field.value_children = _build_fields_SnapshotEntry();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.generated.setting.config.snapshot";
    out.root_name = "ConfigSnapshot";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ConfigSnapshot();
    return out;
}

struct SnapshotEntryBuilder;

struct SnapshotEntry {
    static constexpr uint32_t kFieldId_schema_name = 1;
    static constexpr uint32_t kFieldId_payload_binary = 2;
    static constexpr uint32_t kFieldId_debug_text = 3;

    std::string schema_name{};
    std::optional<std::string> payload_binary{std::nullopt};
    std::optional<std::string> debug_text{std::nullopt};

    auto has_payload_binary() const -> bool {
        return payload_binary.has_value();
    }

    auto has_debug_text() const -> bool {
        return debug_text.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> SnapshotEntry {
        auto out = SnapshotEntry{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("schema_name"); it != obj->end()) {
            out.schema_name = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: schema_name");
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
        obj.emplace("schema_name", schema_write(schema_name));
        if (payload_binary.has_value()) {
            obj.emplace("payload_binary", schema_write(*payload_binary));
        }
        if (debug_text.has_value()) {
            obj.emplace("debug_text", schema_write(*debug_text));
        }
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> SnapshotEntry;
};

struct SnapshotEntryView {
    static constexpr size_t kFieldCount = 3;

    SnapshotEntryView() = default;
    explicit SnapshotEntryView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit SnapshotEntryView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_schema_name = 0;
    static constexpr size_t kSlotIndex_payload_binary = 1;
    static constexpr size_t kSlotIndex_debug_text = 2;

    auto Has_schema_name() const -> bool {
        return not view_.is_null(0);
    }

    auto schema_name() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_payload_binary() const -> bool {
        return not view_.is_null(1);
    }

    auto payload_binary() const -> std::string_view {
        auto v = view_.get_string(1);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_debug_text() const -> bool {
        return not view_.is_null(2);
    }

    auto debug_text() const -> std::string_view {
        auto v = view_.get_string(2);
        return v.has_value() ? *v : std::string_view{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct SnapshotEntryBuilder {
    static constexpr size_t kFieldCount = 3;

    SnapshotEntryBuilder() : writer_(3) {}

    void set_schema_name(std::string_view value) { writer_.set_string(0, value); }
    void set_payload_binary(std::string_view value) { writer_.set_string(1, value); }
    void set_debug_text(std::string_view value) { writer_.set_string(2, value); }

    void clear_payload_binary() { writer_.set_null(1); }
    void clear_debug_text() { writer_.set_null(2); }

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

inline auto SnapshotEntry::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = SnapshotEntryBuilder();
    builder.set_schema_name(schema_name);
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

inline auto SnapshotEntry::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = SnapshotEntryBuilder();
    builder.set_schema_name(schema_name);
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

inline auto SnapshotEntry::from_fixed_layout(std::span<const uint8_t> data) -> SnapshotEntry {
    auto view = SnapshotEntryView(data.data(), data.size());
    auto out = SnapshotEntry();
    out.schema_name = view.schema_name();
    if (view.Has_payload_binary()) { out.payload_binary = view.payload_binary(); }
    if (view.Has_debug_text()) { out.debug_text = view.debug_text(); }
    return out;
}

struct ConfigSnapshotBuilder;

struct ConfigSnapshot {
    static constexpr uint32_t kFieldId_entries = 1;

    std::map<std::string, SnapshotEntry> entries{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> ConfigSnapshot {
        auto out = ConfigSnapshot{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("entries"); it != obj->end()) {
            out.entries = schema_read<std::map<std::string, SnapshotEntry>>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: entries");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("entries", schema_write(entries));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ConfigSnapshot {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> ConfigSnapshot;
};

struct ConfigSnapshotView {
    static constexpr size_t kFieldCount = 1;

    ConfigSnapshotView() = default;
    explicit ConfigSnapshotView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit ConfigSnapshotView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ConfigSnapshotView {
        return ConfigSnapshotView(bytes);
    }

    static constexpr size_t kSlotIndex_entries = 0;

    auto Has_entries() const -> bool {
        return not view_.is_null(0);
    }

    auto entries_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_map_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct ConfigSnapshotBuilder {
    static constexpr size_t kFieldCount = 1;

    ConfigSnapshotBuilder() : writer_(1) {}

    void set_entries_bytes(std::span<const uint8_t> value) { writer_.set_map(0, value); }

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

inline auto ConfigSnapshot::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = ConfigSnapshotBuilder();
    { auto bytes = star::transport::encode_object_map(entries); builder.set_entries_bytes(bytes); }
    return builder.Serialize();
}

inline auto ConfigSnapshot::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = ConfigSnapshotBuilder();
    { auto bytes = star::transport::encode_object_map(entries); builder.set_entries_bytes(bytes); }
    return builder.EncodeTo(buf, cap);
}

inline auto ConfigSnapshot::from_fixed_layout(std::span<const uint8_t> data) -> ConfigSnapshot {
    auto view = ConfigSnapshotView(data.data(), data.size());
    auto out = ConfigSnapshot();
    { auto bytes = view.entries_bytes(); out.entries = star::transport::decode_object_map<SnapshotEntry>(bytes); }
    return out;
}

using ConfigSnapshotSchema = ConfigSnapshot;

}  // namespace star::generated::setting::config::snapshot

#endif // __STAR_SETTING_CONFIG_SNAPSHOT__

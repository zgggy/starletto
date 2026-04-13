#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_CONFIG_SNAPSHOT_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_CONFIG_SNAPSHOT_S_H__

#include "basement/schema/binary_codec.h"
#include "basement/schema/schema_ast.h"
#include "basement/schema/schema_registry.h"
#include "libraries/common/schema/schema_value.h"

#include <concepts>
#include <cstring>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace star::generated::setting::config::snapshot {

template <typename _ValueType>
concept SchemaStructType = requires(const star::schema::SchemaValue& value, const _ValueType& item) {
    { _ValueType::from_value(value) } -> std::same_as<_ValueType>;
    { item.to_value() } -> std::same_as<star::schema::SchemaValue>;
};

template <typename _ValueType>
struct IsStdVector : std::false_type {
};

template <typename _ItemType, typename _Allocator>
struct IsStdVector<std::vector<_ItemType, _Allocator>> : std::true_type {
    using Item = _ItemType;
};

template <typename _ValueType>
struct IsStdMap : std::false_type {
};

template <typename _MappedType, typename _Compare, typename _Allocator>
struct IsStdMap<std::map<std::string, _MappedType, _Compare, _Allocator>> : std::true_type {
    using Item = _MappedType;
};

template <typename _ValueType>
auto schema_read(const star::schema::SchemaValue& value) -> _ValueType {
    if constexpr (IsStdVector<_ValueType>::value) {
        using _Item = typename IsStdVector<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& arr = value.as_array();
        out.reserve(arr.size());
        for (const auto& item : arr) {
            out.emplace_back(schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (IsStdMap<_ValueType>::value) {
        using _Item = typename IsStdMap<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& obj = value.as_object();
        for (const auto& [key, item] : obj) {
            out.emplace(key, schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (std::is_same_v<_ValueType, std::string>) {
        return value.as<std::string>();
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, bool>) {
        return value.as<bool>();
    } else if constexpr (std::is_same_v<_ValueType, int64_t>) {
        return value.as<int64_t>();
    } else if constexpr (std::is_same_v<_ValueType, double>) {
        return value.as<double>();
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaObject>) {
        return value.as_object();
    } else if constexpr (SchemaStructType<_ValueType>) {
        return _ValueType::from_value(value);
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_read type");
    }
}

template <typename _ValueType>
auto schema_write(const _ValueType& value) -> star::schema::SchemaValue {
    if constexpr (IsStdVector<_ValueType>::value) {
        auto out = star::schema::SchemaArray{};
        out.reserve(value.size());
        for (const auto& item : value) {
            out.emplace_back(schema_write(item));
        }
        return star::schema::SchemaValue(std::move(out));
    } else if constexpr (IsStdMap<_ValueType>::value) {
        auto out = star::schema::SchemaObject{};
        for (const auto& [key, item] : value) {
            out.emplace(key, schema_write(item));
        }
        return star::schema::SchemaValue(std::move(out));
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, std::string> || std::is_same_v<_ValueType, bool> ||
                  std::is_same_v<_ValueType, int64_t> || std::is_same_v<_ValueType, double>) {
        return star::schema::schema_value_from(value);
    } else if constexpr (std::is_same_v<_ValueType, star::schema::SchemaObject>) {
        return star::schema::SchemaValue(value);
    } else if constexpr (SchemaStructType<_ValueType>) {
        return value.to_value();
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_write type");
    }
}

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
};

struct SnapshotEntryView {
    SnapshotEntry value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> SnapshotEntryView {
        return SnapshotEntryView{SnapshotEntry::from_value(value)};
    }

    auto Has_schema_name() const -> bool {
        return true;
    }

    auto schema_name() const -> const std::string& {
        return value.schema_name;
    }

    auto Has_payload_binary() const -> bool {
        return value.has_payload_binary();
    }

    auto payload_binary() const -> const std::string& {
        if (not value.payload_binary.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: payload_binary");
        }
        return *value.payload_binary;
    }

    auto Has_debug_text() const -> bool {
        return value.has_debug_text();
    }

    auto debug_text() const -> const std::string& {
        if (not value.debug_text.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: debug_text");
        }
        return *value.debug_text;
    }

};

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
};

struct ConfigSnapshotView {
    ConfigSnapshot value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ConfigSnapshotView {
        return ConfigSnapshotView{ConfigSnapshot::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ConfigSnapshotView {
        return ConfigSnapshotView{ConfigSnapshot::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_entries() const -> bool {
        return true;
    }

    auto entries() const -> const std::map<std::string, SnapshotEntry>& {
        return value.entries;
    }

};

struct ConfigSnapshotBuilder {
    ConfigSnapshot value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const ConfigSnapshot& value) -> ConfigSnapshotBuilder {
        return ConfigSnapshotBuilder{value, value.unknown_fields()};
    }

    void set_unknown_fields(star::schema::UnknownFieldSet value_in) {
        unknown_fields = std::move(value_in);
    }

    auto EncodeTo(uint8_t* buf, size_t cap) const -> size_t {
        if (buf == nullptr) {
            return 0;
        }
        auto tmp = value;
        tmp.set_unknown_fields(unknown_fields);
        const auto bytes = tmp.encode_binary();
        if (bytes.size() > cap) {
            return 0;
        }
        if (not bytes.empty()) {
            std::memcpy(buf, bytes.data(), bytes.size());
        }
        return bytes.size();
    }

    auto EncodeTo(std::span<uint8_t> out) const -> size_t {
        return EncodeTo(out.data(), out.size());
    }
};

using ConfigSnapshotSchema = ConfigSnapshot;

}  // namespace star::generated::setting::config::snapshot

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_CONFIG_SNAPSHOT_S_H__

#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_CONTROL_COMMAND_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_CONTROL_COMMAND_S_H__

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

namespace star::setting::control {

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
    out.name = "control.command";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_ControlCommand() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "throttle";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "brake";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "steering";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "gear";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "frame_id";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int64";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "timestamp_ns";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int64";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.control";
    out.root_name = "ControlCommand";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_ControlCommand();
    return out;
}

struct ControlCommand {
    static constexpr uint32_t kFieldId_throttle = 1;
    static constexpr uint32_t kFieldId_brake = 2;
    static constexpr uint32_t kFieldId_steering = 3;
    static constexpr uint32_t kFieldId_gear = 4;
    static constexpr uint32_t kFieldId_frame_id = 5;
    static constexpr uint32_t kFieldId_timestamp_ns = 6;

    double throttle{};
    double brake{};
    double steering{};
    int64_t gear{};
    int64_t frame_id{};
    int64_t timestamp_ns{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> ControlCommand {
        auto out = ControlCommand{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("throttle"); it != obj->end()) {
            out.throttle = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: throttle");
        }
        if (const auto it = obj->find("brake"); it != obj->end()) {
            out.brake = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: brake");
        }
        if (const auto it = obj->find("steering"); it != obj->end()) {
            out.steering = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: steering");
        }
        if (const auto it = obj->find("gear"); it != obj->end()) {
            out.gear = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: gear");
        }
        if (const auto it = obj->find("frame_id"); it != obj->end()) {
            out.frame_id = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: frame_id");
        }
        if (const auto it = obj->find("timestamp_ns"); it != obj->end()) {
            out.timestamp_ns = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: timestamp_ns");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("throttle", schema_write(throttle));
        obj.emplace("brake", schema_write(brake));
        obj.emplace("steering", schema_write(steering));
        obj.emplace("gear", schema_write(gear));
        obj.emplace("frame_id", schema_write(frame_id));
        obj.emplace("timestamp_ns", schema_write(timestamp_ns));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> ControlCommand {
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

struct ControlCommandView {
    ControlCommand value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ControlCommandView {
        return ControlCommandView{ControlCommand::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> ControlCommandView {
        return ControlCommandView{ControlCommand::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_throttle() const -> bool {
        return true;
    }

    auto throttle() const -> const double& {
        return value.throttle;
    }

    auto Has_brake() const -> bool {
        return true;
    }

    auto brake() const -> const double& {
        return value.brake;
    }

    auto Has_steering() const -> bool {
        return true;
    }

    auto steering() const -> const double& {
        return value.steering;
    }

    auto Has_gear() const -> bool {
        return true;
    }

    auto gear() const -> const int64_t& {
        return value.gear;
    }

    auto Has_frame_id() const -> bool {
        return true;
    }

    auto frame_id() const -> const int64_t& {
        return value.frame_id;
    }

    auto Has_timestamp_ns() const -> bool {
        return true;
    }

    auto timestamp_ns() const -> const int64_t& {
        return value.timestamp_ns;
    }

};

struct ControlCommandBuilder {
    ControlCommand value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const ControlCommand& value) -> ControlCommandBuilder {
        return ControlCommandBuilder{value, value.unknown_fields()};
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

using ControlCommandSchema = ControlCommand;

}  // namespace star::setting::control

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_CONTROL_COMMAND_S_H__

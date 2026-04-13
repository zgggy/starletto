#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_PLANNING_MOTION_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_PLANNING_MOTION_S_H__

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

namespace star::setting::parameter::planning {

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
    out.name = "planning.motion";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_Processor() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "expected_cycle_time";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "log_level";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_Methods() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "trajectory_method";
        field.kind = star::schema::FieldKind::STRING;
        field.type_name = "string";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "enable_fallback";
        field.kind = star::schema::FieldKind::BOOL;
        field.type_name = "bool";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_MotionParameter() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "processor";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Processor";
        field.optional = false;
        field.repeated = false;
        field.children = _build_fields_Processor();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "methods";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Methods";
        field.optional = true;
        field.repeated = false;
        field.children = _build_fields_Methods();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.parameter.planning";
    out.root_name = "MotionParameter";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_MotionParameter();
    return out;
}

struct Processor {
    static constexpr uint32_t kFieldId_expected_cycle_time = 1;
    static constexpr uint32_t kFieldId_log_level = 2;

    int64_t expected_cycle_time{};
    std::string log_level{};

    static auto from_value(const star::schema::SchemaValue& value) -> Processor {
        auto out = Processor{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("expected_cycle_time"); it != obj->end()) {
            out.expected_cycle_time = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: expected_cycle_time");
        }
        if (const auto it = obj->find("log_level"); it != obj->end()) {
            out.log_level = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: log_level");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("expected_cycle_time", schema_write(expected_cycle_time));
        obj.emplace("log_level", schema_write(log_level));
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct ProcessorView {
    Processor value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> ProcessorView {
        return ProcessorView{Processor::from_value(value)};
    }

    auto Has_expected_cycle_time() const -> bool {
        return true;
    }

    auto expected_cycle_time() const -> const int64_t& {
        return value.expected_cycle_time;
    }

    auto Has_log_level() const -> bool {
        return true;
    }

    auto log_level() const -> const std::string& {
        return value.log_level;
    }

};

struct Methods {
    static constexpr uint32_t kFieldId_trajectory_method = 1;
    static constexpr uint32_t kFieldId_enable_fallback = 2;

    std::string trajectory_method{};
    bool enable_fallback{};

    static auto from_value(const star::schema::SchemaValue& value) -> Methods {
        auto out = Methods{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("trajectory_method"); it != obj->end()) {
            out.trajectory_method = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: trajectory_method");
        }
        if (const auto it = obj->find("enable_fallback"); it != obj->end()) {
            out.enable_fallback = schema_read<bool>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: enable_fallback");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("trajectory_method", schema_write(trajectory_method));
        obj.emplace("enable_fallback", schema_write(enable_fallback));
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct MethodsView {
    Methods value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> MethodsView {
        return MethodsView{Methods::from_value(value)};
    }

    auto Has_trajectory_method() const -> bool {
        return true;
    }

    auto trajectory_method() const -> const std::string& {
        return value.trajectory_method;
    }

    auto Has_enable_fallback() const -> bool {
        return true;
    }

    auto enable_fallback() const -> const bool& {
        return value.enable_fallback;
    }

};

struct MotionParameter {
    static constexpr uint32_t kFieldId_processor = 1;
    static constexpr uint32_t kFieldId_methods = 2;

    Processor processor{};
    std::optional<Methods> methods{std::nullopt};
    star::schema::UnknownFieldSet unknown_fields_{};

    auto has_methods() const -> bool {
        return methods.has_value();
    }

    static auto from_value(const star::schema::SchemaValue& value) -> MotionParameter {
        auto out = MotionParameter{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("processor"); it != obj->end()) {
            out.processor = schema_read<Processor>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: processor");
        }
        if (const auto it = obj->find("methods"); it != obj->end()) {
            out.methods = schema_read<Methods>(it->second);
        } else {
            out.methods.reset();
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("processor", schema_write(processor));
        if (methods.has_value()) {
            obj.emplace("methods", schema_write(*methods));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> MotionParameter {
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

struct MotionParameterView {
    MotionParameter value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> MotionParameterView {
        return MotionParameterView{MotionParameter::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> MotionParameterView {
        return MotionParameterView{MotionParameter::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_processor() const -> bool {
        return true;
    }

    auto processor() const -> const Processor& {
        return value.processor;
    }

    auto Has_methods() const -> bool {
        return value.has_methods();
    }

    auto methods() const -> const Methods& {
        if (not value.methods.has_value()) {
            throw star::excep::RuntimeError("schema generated view access failed, missing field: methods");
        }
        return *value.methods;
    }

};

struct MotionParameterBuilder {
    MotionParameter value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const MotionParameter& value) -> MotionParameterBuilder {
        return MotionParameterBuilder{value, value.unknown_fields()};
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

using MotionParameterSchema = MotionParameter;

}  // namespace star::setting::parameter::planning

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_PLANNING_MOTION_S_H__

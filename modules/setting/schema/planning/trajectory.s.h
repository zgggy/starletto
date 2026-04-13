#ifndef ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_PLANNING_TRAJECTORY_S_H__
#define ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_PLANNING_TRAJECTORY_S_H__

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

namespace star::setting::planning {

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
    out.name = "planning.trajectory";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_TrajectoryPoint() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "x";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "y";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
        field.name = "heading";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 4;
        field.name = "velocity";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 5;
        field.name = "curvature";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 6;
        field.name = "relative_time_s";
        field.kind = star::schema::FieldKind::FLOAT64;
        field.type_name = "double";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_TrajectoryOutput() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "points";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "TrajectoryPoint";
        field.optional = false;
        field.repeated = true;
        field.children = _build_fields_TrajectoryPoint();
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 2;
        field.name = "frame_id";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int64";
        field.optional = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 3;
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
    out.namespace_name = "star.setting.planning";
    out.root_name = "TrajectoryOutput";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_TrajectoryOutput();
    return out;
}

struct TrajectoryPoint {
    static constexpr uint32_t kFieldId_x = 1;
    static constexpr uint32_t kFieldId_y = 2;
    static constexpr uint32_t kFieldId_heading = 3;
    static constexpr uint32_t kFieldId_velocity = 4;
    static constexpr uint32_t kFieldId_curvature = 5;
    static constexpr uint32_t kFieldId_relative_time_s = 6;

    double x{};
    double y{};
    double heading{};
    double velocity{};
    double curvature{};
    double relative_time_s{};

    static auto from_value(const star::schema::SchemaValue& value) -> TrajectoryPoint {
        auto out = TrajectoryPoint{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("x"); it != obj->end()) {
            out.x = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: x");
        }
        if (const auto it = obj->find("y"); it != obj->end()) {
            out.y = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: y");
        }
        if (const auto it = obj->find("heading"); it != obj->end()) {
            out.heading = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: heading");
        }
        if (const auto it = obj->find("velocity"); it != obj->end()) {
            out.velocity = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: velocity");
        }
        if (const auto it = obj->find("curvature"); it != obj->end()) {
            out.curvature = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: curvature");
        }
        if (const auto it = obj->find("relative_time_s"); it != obj->end()) {
            out.relative_time_s = schema_read<double>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: relative_time_s");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("x", schema_write(x));
        obj.emplace("y", schema_write(y));
        obj.emplace("heading", schema_write(heading));
        obj.emplace("velocity", schema_write(velocity));
        obj.emplace("curvature", schema_write(curvature));
        obj.emplace("relative_time_s", schema_write(relative_time_s));
        return star::schema::SchemaValue(std::move(obj));
    }
};

struct TrajectoryPointView {
    TrajectoryPoint value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> TrajectoryPointView {
        return TrajectoryPointView{TrajectoryPoint::from_value(value)};
    }

    auto Has_x() const -> bool {
        return true;
    }

    auto x() const -> const double& {
        return value.x;
    }

    auto Has_y() const -> bool {
        return true;
    }

    auto y() const -> const double& {
        return value.y;
    }

    auto Has_heading() const -> bool {
        return true;
    }

    auto heading() const -> const double& {
        return value.heading;
    }

    auto Has_velocity() const -> bool {
        return true;
    }

    auto velocity() const -> const double& {
        return value.velocity;
    }

    auto Has_curvature() const -> bool {
        return true;
    }

    auto curvature() const -> const double& {
        return value.curvature;
    }

    auto Has_relative_time_s() const -> bool {
        return true;
    }

    auto relative_time_s() const -> const double& {
        return value.relative_time_s;
    }

};

struct TrajectoryOutput {
    static constexpr uint32_t kFieldId_points = 1;
    static constexpr uint32_t kFieldId_frame_id = 2;
    static constexpr uint32_t kFieldId_timestamp_ns = 3;

    std::vector<TrajectoryPoint> points{};
    int64_t frame_id{};
    int64_t timestamp_ns{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> TrajectoryOutput {
        auto out = TrajectoryOutput{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("points"); it != obj->end()) {
            out.points = schema_read<std::vector<TrajectoryPoint>>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: points");
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
        obj.emplace("points", schema_write(points));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> TrajectoryOutput {
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

struct TrajectoryOutputView {
    TrajectoryOutput value{};

    static auto FromValue(const star::schema::SchemaValue& value) -> TrajectoryOutputView {
        return TrajectoryOutputView{TrajectoryOutput::from_value(value)};
    }

    static auto FromBinary(std::span<const uint8_t> bytes) -> TrajectoryOutputView {
        return TrajectoryOutputView{TrajectoryOutput::from_binary(bytes)};
    }

    auto unknown_fields() const -> const star::schema::UnknownFieldSet& {
        return value.unknown_fields();
    }

    auto Has_points() const -> bool {
        return true;
    }

    auto points() const -> const std::vector<TrajectoryPoint>& {
        return value.points;
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

struct TrajectoryOutputBuilder {
    TrajectoryOutput value{};
    star::schema::UnknownFieldSet unknown_fields{};

    static auto FromValue(const TrajectoryOutput& value) -> TrajectoryOutputBuilder {
        return TrajectoryOutputBuilder{value, value.unknown_fields()};
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

using TrajectoryOutputSchema = TrajectoryOutput;

}  // namespace star::setting::planning

#endif // ___HOME_ZGY_PROJECTS_STARLETTO_MODULES_SETTING_SCHEMA_PLANNING_TRAJECTORY_S_H__

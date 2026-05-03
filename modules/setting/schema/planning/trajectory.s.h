#ifndef __PLANNING_TRAJECTORY__
#define __PLANNING_TRAJECTORY__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::planning {

using star::schema::schema_read;
using star::schema::schema_write;

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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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
        field.explicit_required = false;
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

struct TrajectoryPointBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> TrajectoryPoint;
};

struct TrajectoryPointView {
    static constexpr size_t kFieldCount = 6;

    TrajectoryPointView() = default;
    explicit TrajectoryPointView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit TrajectoryPointView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_x = 0;
    static constexpr size_t kSlotIndex_y = 1;
    static constexpr size_t kSlotIndex_heading = 2;
    static constexpr size_t kSlotIndex_velocity = 3;
    static constexpr size_t kSlotIndex_curvature = 4;
    static constexpr size_t kSlotIndex_relative_time_s = 5;

    auto Has_x() const -> bool {
        return not view_.is_null(0);
    }

    auto x() const -> double {
        auto v = view_.get_float64(0);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_y() const -> bool {
        return not view_.is_null(1);
    }

    auto y() const -> double {
        auto v = view_.get_float64(1);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_heading() const -> bool {
        return not view_.is_null(2);
    }

    auto heading() const -> double {
        auto v = view_.get_float64(2);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_velocity() const -> bool {
        return not view_.is_null(3);
    }

    auto velocity() const -> double {
        auto v = view_.get_float64(3);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_curvature() const -> bool {
        return not view_.is_null(4);
    }

    auto curvature() const -> double {
        auto v = view_.get_float64(4);
        return v.has_value() ? *v : 0.0;
    }

    auto Has_relative_time_s() const -> bool {
        return not view_.is_null(5);
    }

    auto relative_time_s() const -> double {
        auto v = view_.get_float64(5);
        return v.has_value() ? *v : 0.0;
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct TrajectoryPointBuilder {
    static constexpr size_t kFieldCount = 6;

    TrajectoryPointBuilder() : writer_(6) {}

    void set_x(double value) { writer_.set_float64(0, value); }
    void set_y(double value) { writer_.set_float64(1, value); }
    void set_heading(double value) { writer_.set_float64(2, value); }
    void set_velocity(double value) { writer_.set_float64(3, value); }
    void set_curvature(double value) { writer_.set_float64(4, value); }
    void set_relative_time_s(double value) { writer_.set_float64(5, value); }

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

inline auto TrajectoryPoint::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = TrajectoryPointBuilder();
    builder.set_x(x);
    builder.set_y(y);
    builder.set_heading(heading);
    builder.set_velocity(velocity);
    builder.set_curvature(curvature);
    builder.set_relative_time_s(relative_time_s);
    return builder.Serialize();
}

inline auto TrajectoryPoint::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = TrajectoryPointBuilder();
    builder.set_x(x);
    builder.set_y(y);
    builder.set_heading(heading);
    builder.set_velocity(velocity);
    builder.set_curvature(curvature);
    builder.set_relative_time_s(relative_time_s);
    return builder.EncodeTo(buf, cap);
}

inline auto TrajectoryPoint::from_fixed_layout(std::span<const uint8_t> data) -> TrajectoryPoint {
    auto view = TrajectoryPointView(data.data(), data.size());
    auto out = TrajectoryPoint();
    out.x = view.x();
    out.y = view.y();
    out.heading = view.heading();
    out.velocity = view.velocity();
    out.curvature = view.curvature();
    out.relative_time_s = view.relative_time_s();
    return out;
}

struct TrajectoryOutputBuilder;

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

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> TrajectoryOutput;
};

struct TrajectoryOutputView {
    static constexpr size_t kFieldCount = 3;

    TrajectoryOutputView() = default;
    explicit TrajectoryOutputView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit TrajectoryOutputView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> TrajectoryOutputView {
        return TrajectoryOutputView(bytes);
    }

    static constexpr size_t kSlotIndex_points = 0;
    static constexpr size_t kSlotIndex_frame_id = 1;
    static constexpr size_t kSlotIndex_timestamp_ns = 2;

    auto Has_points() const -> bool {
        return not view_.is_null(0);
    }

    auto points_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_array_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto Has_frame_id() const -> bool {
        return not view_.is_null(1);
    }

    auto frame_id() const -> int64_t {
        auto v = view_.get_int64(1);
        return v.has_value() ? *v : int64_t{0};
    }

    auto Has_timestamp_ns() const -> bool {
        return not view_.is_null(2);
    }

    auto timestamp_ns() const -> int64_t {
        auto v = view_.get_int64(2);
        return v.has_value() ? *v : int64_t{0};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct TrajectoryOutputBuilder {
    static constexpr size_t kFieldCount = 3;

    TrajectoryOutputBuilder() : writer_(3) {}

    void set_points_bytes(std::span<const uint8_t> value) { writer_.set_array(0, value); }
    void set_frame_id(int64_t value) { writer_.set_int64(1, value); }
    void set_timestamp_ns(int64_t value) { writer_.set_int64(2, value); }

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

inline auto TrajectoryOutput::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = TrajectoryOutputBuilder();
    { auto bytes = star::transport::encode_object_array(points); builder.set_points_bytes(bytes); }
    builder.set_frame_id(frame_id);
    builder.set_timestamp_ns(timestamp_ns);
    return builder.Serialize();
}

inline auto TrajectoryOutput::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = TrajectoryOutputBuilder();
    { auto bytes = star::transport::encode_object_array(points); builder.set_points_bytes(bytes); }
    builder.set_frame_id(frame_id);
    builder.set_timestamp_ns(timestamp_ns);
    return builder.EncodeTo(buf, cap);
}

inline auto TrajectoryOutput::from_fixed_layout(std::span<const uint8_t> data) -> TrajectoryOutput {
    auto view = TrajectoryOutputView(data.data(), data.size());
    auto out = TrajectoryOutput();
    { auto bytes = view.points_bytes(); out.points = star::transport::decode_object_array<TrajectoryPoint>(bytes); }
    out.frame_id = view.frame_id();
    out.timestamp_ns = view.timestamp_ns();
    return out;
}

using TrajectoryOutputSchema = TrajectoryOutput;

}  // namespace star::setting::planning

#endif // __PLANNING_TRAJECTORY__

#ifndef __LOGGER_LOGGER__
#define __LOGGER_LOGGER__

#include "basement/schema/schema_codec_helpers.h"
#include "basement/schema/schema_registry.h"
#include "basement/transport/common/fixed_layout_codec.h"

#include <cstring>
#include <optional>
#include <span>
#include <utility>

namespace star::setting::config::logger {

using star::schema::schema_read;
using star::schema::schema_write;

inline auto _schema_type_id() -> star::schema::SchemaTypeId {
    auto out = star::schema::SchemaTypeId{};
    out.name = "logger.logger";
    out.version = 1;
    out.hash = static_cast<uint64_t>(0);
    return out;
}

inline auto _build_fields_Logger() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "level";
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
        field.name = "flush_interval_ms";
        field.kind = star::schema::FieldKind::INT64;
        field.type_name = "int";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _build_fields_LoggerConfig() -> std::vector<star::schema::FieldDescriptor> {
    auto out = std::vector<star::schema::FieldDescriptor>{};
    {
        auto field = star::schema::FieldDescriptor{};
        field.id = 1;
        field.name = "logger";
        field.kind = star::schema::FieldKind::OBJECT;
        field.type_name = "Logger";
        field.optional = false;
        field.explicit_required = false;
        field.repeated = false;
        field.children = _build_fields_Logger();
        out.emplace_back(std::move(field));
    }
    return out;
}

inline auto _schema_descriptor() -> star::schema::SchemaDescriptor {
    auto out = star::schema::SchemaDescriptor{};
    out.type_id = _schema_type_id();
    out.namespace_name = "star.setting.config.logger";
    out.root_name = "LoggerConfig";
    out.allow_unknown_fields = false;
    out.fields = _build_fields_LoggerConfig();
    return out;
}

struct LoggerBuilder;

struct Logger {
    static constexpr uint32_t kFieldId_level = 1;
    static constexpr uint32_t kFieldId_flush_interval_ms = 2;

    std::string level{};
    int64_t flush_interval_ms{};

    static auto from_value(const star::schema::SchemaValue& value) -> Logger {
        auto out = Logger{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("level"); it != obj->end()) {
            out.level = schema_read<std::string>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: level");
        }
        if (const auto it = obj->find("flush_interval_ms"); it != obj->end()) {
            out.flush_interval_ms = schema_read<int64_t>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: flush_interval_ms");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("level", schema_write(level));
        obj.emplace("flush_interval_ms", schema_write(flush_interval_ms));
        return star::schema::SchemaValue(std::move(obj));
    }

    auto encode_fixed_layout() const -> std::vector<uint8_t>;

    auto encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t;

    static auto from_fixed_layout(std::span<const uint8_t> data) -> Logger;
};

struct LoggerView {
    static constexpr size_t kFieldCount = 2;

    LoggerView() = default;
    explicit LoggerView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit LoggerView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static constexpr size_t kSlotIndex_level = 0;
    static constexpr size_t kSlotIndex_flush_interval_ms = 1;

    auto Has_level() const -> bool {
        return not view_.is_null(0);
    }

    auto level() const -> std::string_view {
        auto v = view_.get_string(0);
        return v.has_value() ? *v : std::string_view{};
    }

    auto Has_flush_interval_ms() const -> bool {
        return not view_.is_null(1);
    }

    auto flush_interval_ms() const -> int64_t {
        auto v = view_.get_int64(1);
        return v.has_value() ? *v : int64_t{0};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct LoggerBuilder {
    static constexpr size_t kFieldCount = 2;

    LoggerBuilder() : writer_(2) {}

    void set_level(std::string_view value) { writer_.set_string(0, value); }
    void set_flush_interval_ms(int64_t value) { writer_.set_int64(1, value); }

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

inline auto Logger::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = LoggerBuilder();
    builder.set_level(level);
    builder.set_flush_interval_ms(flush_interval_ms);
    return builder.Serialize();
}

inline auto Logger::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = LoggerBuilder();
    builder.set_level(level);
    builder.set_flush_interval_ms(flush_interval_ms);
    return builder.EncodeTo(buf, cap);
}

inline auto Logger::from_fixed_layout(std::span<const uint8_t> data) -> Logger {
    auto view = LoggerView(data.data(), data.size());
    auto out = Logger();
    out.level = view.level();
    out.flush_interval_ms = view.flush_interval_ms();
    return out;
}

struct LoggerConfigBuilder;

struct LoggerConfig {
    static constexpr uint32_t kFieldId_logger = 1;

    Logger logger{};
    star::schema::UnknownFieldSet unknown_fields_{};

    static auto from_value(const star::schema::SchemaValue& value) -> LoggerConfig {
        auto out = LoggerConfig{};
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("schema generated type decode failed, value is not object");
        }
        if (const auto it = obj->find("logger"); it != obj->end()) {
            out.logger = schema_read<Logger>(it->second);
        } else {
            throw star::excep::RuntimeError("schema generated type decode failed, missing field: logger");
        }
        return out;
    }

    auto to_value() const -> star::schema::SchemaValue {
        auto obj = star::schema::SchemaObject{};
        obj.emplace("logger", schema_write(logger));
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

    static auto from_binary(std::span<const uint8_t> bytes) -> LoggerConfig {
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

    static auto from_fixed_layout(std::span<const uint8_t> data) -> LoggerConfig;
};

struct LoggerConfigView {
    static constexpr size_t kFieldCount = 1;

    LoggerConfigView() = default;
    explicit LoggerConfigView(const uint8_t* data, size_t size) : view_(data, size) {}
    explicit LoggerConfigView(std::span<const uint8_t> bytes) : view_(bytes.data(), bytes.size()) {}

    auto valid() const -> bool { return view_.valid(); }

    static auto FromBinary(std::span<const uint8_t> bytes) -> LoggerConfigView {
        return LoggerConfigView(bytes);
    }

    static constexpr size_t kSlotIndex_logger = 0;

    auto Has_logger() const -> bool {
        return not view_.is_null(0);
    }

    auto logger_bytes() const -> std::span<const uint8_t> {
        auto v = view_.get_object_bytes(0);
        return v.has_value() ? *v : std::span<const uint8_t>{};
    }

    auto raw_view() const -> const star::transport::FixedLayoutView& { return view_; }

  private:
    star::transport::FixedLayoutView view_;
};

struct LoggerConfigBuilder {
    static constexpr size_t kFieldCount = 1;

    LoggerConfigBuilder() : writer_(1) {}

    void set_logger_bytes(std::span<const uint8_t> value) { writer_.set_object(0, value); }

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

inline auto LoggerConfig::encode_fixed_layout() const -> std::vector<uint8_t> {
    auto builder = LoggerConfigBuilder();
    { auto bytes = logger.encode_fixed_layout(); builder.set_logger_bytes(bytes); }
    return builder.Serialize();
}

inline auto LoggerConfig::encode_fixed_layout_into(uint8_t* buf, size_t cap) const -> size_t {
    auto builder = LoggerConfigBuilder();
    { auto bytes = logger.encode_fixed_layout(); builder.set_logger_bytes(bytes); }
    return builder.EncodeTo(buf, cap);
}

inline auto LoggerConfig::from_fixed_layout(std::span<const uint8_t> data) -> LoggerConfig {
    auto view = LoggerConfigView(data.data(), data.size());
    auto out = LoggerConfig();
    { auto bytes = view.logger_bytes(); out.logger = Logger::from_fixed_layout(bytes); }
    return out;
}

using LoggerConfigSchema = LoggerConfig;

}  // namespace star::setting::config::logger

#endif // __LOGGER_LOGGER__

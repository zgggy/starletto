#ifndef __SCHEMA_BINARY_CODEC_H__
#define __SCHEMA_BINARY_CODEC_H__

#include "basement/core/exception.h"
#include "basement/schema/schema_ast.h"
#include "libraries/common/schema/schema_value.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace star::schema {

struct UnknownField {
    uint32_t field_id{0};
    uint8_t wire_type{0};
    std::vector<uint8_t> tlv_bytes{};
};

using UnknownFieldSet = std::vector<UnknownField>;

struct DecodedMessage {
    star::schema::SchemaValue value{};
    UnknownFieldSet unknown_fields{};
};

class BinaryCodec {
public:
    static auto encode(
        const SchemaDescriptor& schema,
        const star::schema::SchemaValue& value,
        const UnknownFieldSet& unknown_fields = {}) -> std::vector<uint8_t> {
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, root value is not object");
        }
        std::vector<uint8_t> out;
        encode_object(schema.root_name, schema.fields, *obj, &out);
        for (const auto& item : unknown_fields) {
            out.insert(out.end(), item.tlv_bytes.begin(), item.tlv_bytes.end());
        }
        return out;
    }

    static auto decode(const SchemaDescriptor& schema, std::span<const uint8_t> bytes) -> star::schema::SchemaValue {
        return decode_with_unknown(schema, bytes).value;
    }

    static auto decode_with_unknown(const SchemaDescriptor& schema, std::span<const uint8_t> bytes) -> DecodedMessage {
        star::schema::SchemaObject obj;
        UnknownFieldSet unknown_fields;
        decode_object(schema.root_name, schema.fields, bytes, &obj, &unknown_fields);
        return DecodedMessage{
            .value = star::schema::SchemaValue(std::move(obj)),
            .unknown_fields = std::move(unknown_fields),
        };
    }

private:
    enum class WireType : uint8_t {
        VARINT = 0,
        FIXED64 = 1,
        BYTES = 2,
        FIXED32 = 5,
    };

    struct Cursor {
        std::span<const uint8_t> bytes;
        size_t pos{0};
    };

    static void encode_object(
        const std::string& scope,
        const std::vector<FieldDescriptor>& fields,
        const star::schema::SchemaObject& obj,
        std::vector<uint8_t>* out) {
        if (out == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec encode_object failed, output is null");
        }
        for (const auto& field : fields) {
            const auto it = obj.find(field.name);
            if (it == obj.end()) {
                continue;
            }
            encode_field(scope + "." + field.name, field, it->second, out);
        }
    }

    static void encode_field(const std::string& path, const FieldDescriptor& field, const star::schema::SchemaValue& value, std::vector<uint8_t>* out) {
        if (field.id == 0) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, missing field id: ", path);
        }
        if (field.repeated) {
            const auto* arr = value.get_array_ptr();
            if (arr == nullptr) {
                throw star::excep::RuntimeError("BinaryCodec encode failed, repeated field is not array: ", path);
            }
            for (const auto& item : *arr) {
                auto item_field = field;
                item_field.repeated = false;
                encode_field(path, item_field, item, out);
            }
            return;
        }
        if (field.has_value_kind) {
            encode_map(path, field, value, out);
            return;
        }
        if (is_conf_value_field(field)) {
            write_tag(field.id, WireType::BYTES, out);
            const auto text = star::schema::dump_schema_text(value);
            write_varint(text.size(), out);
            out->insert(out->end(), text.begin(), text.end());
            return;
        }

        switch (field.kind) {
        case FieldKind::BOOL:
            write_tag(field.id, WireType::VARINT, out);
            write_varint(as_bool(path, value) ? 1 : 0, out);
            return;
        case FieldKind::INT64:
            write_tag(field.id, WireType::VARINT, out);
            write_varint(static_cast<uint64_t>(as_int64(path, value)), out);
            return;
        case FieldKind::FLOAT64:
            if (field.type_name == "float") {
                write_tag(field.id, WireType::FIXED32, out);
                write_fixed32(std::bit_cast<uint32_t>(static_cast<float>(as_float64(path, value))), out);
                return;
            }
            write_tag(field.id, WireType::FIXED64, out);
            write_fixed64(std::bit_cast<uint64_t>(as_float64(path, value)), out);
            return;
        case FieldKind::STRING: {
            write_tag(field.id, WireType::BYTES, out);
            const auto& s = as_string(path, value);
            write_varint(s.size(), out);
            out->insert(out->end(), s.begin(), s.end());
            return;
        }
        case FieldKind::ARRAY:
            throw star::excep::RuntimeError("BinaryCodec encode failed, ARRAY field kind should use repeated path: ", path);
        case FieldKind::OBJECT:
            encode_nested_object(path, field, value, out);
            return;
        }
    }

    static void encode_nested_object(
        const std::string& path,
        const FieldDescriptor& field,
        const star::schema::SchemaValue& value,
        std::vector<uint8_t>* out) {
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, nested object is not object: ", path);
        }
        if (field.children.empty()) {
            write_tag(field.id, WireType::BYTES, out);
            const auto text = star::schema::dump_schema_text(value);
            write_varint(text.size(), out);
            out->insert(out->end(), text.begin(), text.end());
            return;
        }
        std::vector<uint8_t> nested;
        encode_object(path, field.children, *obj, &nested);
        write_tag(field.id, WireType::BYTES, out);
        write_varint(nested.size(), out);
        out->insert(out->end(), nested.begin(), nested.end());
    }

    static void encode_map(const std::string& path, const FieldDescriptor& field, const star::schema::SchemaValue& value, std::vector<uint8_t>* out) {
        const auto* obj = value.get_object_ptr();
        if (obj == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, map field is not object: ", path);
        }
        for (const auto& [key, item] : *obj) {
            std::vector<uint8_t> entry;
            write_tag(1, WireType::BYTES, &entry);
            write_varint(key.size(), &entry);
            entry.insert(entry.end(), key.begin(), key.end());

            FieldDescriptor value_field;
            value_field.id = 2;
            value_field.kind = field.value_kind;
            value_field.type_name = field.value_type_name;
            value_field.children = field.value_children;
            encode_field(path + "." + key, value_field, item, &entry);

            write_tag(field.id, WireType::BYTES, out);
            write_varint(entry.size(), out);
            out->insert(out->end(), entry.begin(), entry.end());
        }
    }

    static void decode_object(
        const std::string& scope,
        const std::vector<FieldDescriptor>& fields,
        std::span<const uint8_t> bytes,
        star::schema::SchemaObject* out,
        UnknownFieldSet* unknown_fields = nullptr) {
        if (out == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec decode_object failed, output is null");
        }
        std::map<uint32_t, const FieldDescriptor*> by_id;
        for (const auto& field : fields) {
            by_id.emplace(field.id, &field);
        }

        Cursor cur{bytes, 0};
        while (cur.pos < cur.bytes.size()) {
            const auto tag = read_varint(&cur);
            const auto field_id = static_cast<uint32_t>(tag >> 3);
            const auto wire_type = static_cast<uint8_t>(tag & 0x7);
            const auto it = by_id.find(field_id);
            if (it == by_id.end()) {
                const auto tlv = consume_unknown_tlv(tag, field_id, wire_type, &cur);
                if (unknown_fields != nullptr) {
                    unknown_fields->emplace_back(UnknownField{
                        .field_id = field_id,
                        .wire_type = wire_type,
                        .tlv_bytes = tlv,
                    });
                }
                continue;
            }
            decode_field(scope, *it->second, wire_type, &cur, out);
        }
    }

    static void decode_field(
        const std::string& scope,
        const FieldDescriptor& field,
        uint8_t wire_type,
        Cursor* cur,
        star::schema::SchemaObject* out) {
        if (cur == nullptr || out == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec decode_field failed, invalid output");
        }
        const auto path = scope + "." + field.name;
        if (field.repeated) {
            auto& slot = ensure_array_slot(out, field.name);
            auto item_field = field;
            item_field.repeated = false;
            slot.emplace_back(decode_single_value(path, item_field, wire_type, cur));
            return;
        }
        if (field.has_value_kind) {
            auto& slot = ensure_object_slot(out, field.name);
            decode_map_entry(path, field, wire_type, cur, &slot);
            return;
        }
        (*out)[field.name] = decode_single_value(path, field, wire_type, cur);
    }

    static auto decode_single_value(const std::string& path, const FieldDescriptor& field, uint8_t wire_type, Cursor* cur)
        -> star::schema::SchemaValue {
        if (is_conf_value_field(field)) {
            expect_wire_type(path, wire_type, WireType::BYTES);
            const auto data = read_bytes(cur);
            return star::schema::parse_schema_text(std::string(reinterpret_cast<const char*>(data.data()), data.size()));
        }
        switch (field.kind) {
        case FieldKind::BOOL:
            expect_wire_type(path, wire_type, WireType::VARINT);
            return star::schema::SchemaValue(read_varint(cur) != 0);
        case FieldKind::INT64:
            expect_wire_type(path, wire_type, WireType::VARINT);
            return star::schema::SchemaValue(static_cast<int64_t>(read_varint(cur)));
        case FieldKind::FLOAT64:
            if (field.type_name == "float") {
                expect_wire_type(path, wire_type, WireType::FIXED32);
                const auto raw = read_fixed32(cur);
                return star::schema::SchemaValue(static_cast<double>(std::bit_cast<float>(raw)));
            }
            expect_wire_type(path, wire_type, WireType::FIXED64);
            return star::schema::SchemaValue(std::bit_cast<double>(read_fixed64(cur)));
        case FieldKind::STRING: {
            expect_wire_type(path, wire_type, WireType::BYTES);
            const auto data = read_bytes(cur);
            return star::schema::SchemaValue(std::string(reinterpret_cast<const char*>(data.data()), data.size()));
        }
        case FieldKind::ARRAY:
            throw star::excep::RuntimeError("BinaryCodec decode failed, ARRAY field kind should use repeated path: ", path);
        case FieldKind::OBJECT:
            expect_wire_type(path, wire_type, WireType::BYTES);
            if (field.children.empty()) {
                const auto raw = read_bytes(cur);
                auto parsed = star::schema::parse_schema_text(std::string(
                    reinterpret_cast<const char*>(raw.data()),
                    raw.size()));
                const auto* obj = parsed.get_object_ptr();
                if (obj == nullptr) {
                    throw star::excep::RuntimeError("BinaryCodec decode failed, raw object text is not object: ", path);
                }
                return star::schema::SchemaValue(*obj);
            }
            {
                star::schema::SchemaObject obj;
                const auto nested = read_bytes(cur);
                decode_object(path, field.children, nested, &obj, nullptr);
                return star::schema::SchemaValue(std::move(obj));
            }
        }
        throw star::excep::RuntimeError("BinaryCodec decode failed, unknown field kind: ", path);
    }

    static void decode_map_entry(
        const std::string& path,
        const FieldDescriptor& field,
        uint8_t wire_type,
        Cursor* cur,
        star::schema::SchemaObject* out) {
        expect_wire_type(path, wire_type, WireType::BYTES);
        auto bytes = read_bytes(cur);
        Cursor entry{bytes, 0};
        std::string key;
        star::schema::SchemaValue value;
        bool has_key = false;
        bool has_value = false;
        while (entry.pos < entry.bytes.size()) {
            const auto tag = read_varint(&entry);
            const auto field_id = static_cast<uint32_t>(tag >> 3);
            const auto wt = static_cast<uint8_t>(tag & 0x7);
            if (field_id == 1) {
                expect_wire_type(path + ".$key", wt, WireType::BYTES);
                const auto data = read_bytes(&entry);
                key.assign(reinterpret_cast<const char*>(data.data()), data.size());
                has_key = true;
                continue;
            }
            if (field_id == 2) {
                FieldDescriptor value_field;
                value_field.id = 2;
                value_field.kind = field.value_kind;
                value_field.type_name = field.value_type_name;
                value_field.children = field.value_children;
                value = decode_single_value(path + ".$value", value_field, wt, &entry);
                has_value = true;
                continue;
            }
            skip_unknown(path + ".$entry", wt, &entry);
        }
        if (not has_key || not has_value) {
            throw star::excep::RuntimeError("BinaryCodec decode failed, invalid map entry: ", path);
        }
        (*out)[key] = std::move(value);
    }

    static auto ensure_array_slot(star::schema::SchemaObject* obj, const std::string& name) -> star::schema::SchemaArray& {
        auto [it, inserted] = obj->try_emplace(name, star::schema::SchemaArray{});
        if (inserted) {
            return it->second.as_array();
        }
        if (not it->second.is_array()) {
            throw star::excep::RuntimeError("BinaryCodec decode failed, existing slot is not array: ", name);
        }
        return it->second.as_array();
    }

    static auto ensure_object_slot(star::schema::SchemaObject* obj, const std::string& name) -> star::schema::SchemaObject& {
        auto [it, inserted] = obj->try_emplace(name, star::schema::SchemaObject{});
        if (inserted) {
            return it->second.as_object();
        }
        if (not it->second.is_object()) {
            throw star::excep::RuntimeError("BinaryCodec decode failed, existing slot is not object: ", name);
        }
        return it->second.as_object();
    }

    static auto as_bool(const std::string& path, const star::schema::SchemaValue& value) -> bool {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<bool>(*scalar)) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, expect bool: ", path);
        }
        return std::get<bool>(*scalar);
    }

    static auto as_int64(const std::string& path, const star::schema::SchemaValue& value) -> int64_t {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<int64_t>(*scalar)) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, expect int64: ", path);
        }
        return std::get<int64_t>(*scalar);
    }

    static auto as_float64(const std::string& path, const star::schema::SchemaValue& value) -> double {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, expect float64: ", path);
        }
        if (std::holds_alternative<double>(*scalar)) {
            return std::get<double>(*scalar);
        }
        if (std::holds_alternative<int64_t>(*scalar)) {
            return static_cast<double>(std::get<int64_t>(*scalar));
        }
        throw star::excep::RuntimeError("BinaryCodec encode failed, expect float64: ", path);
    }

    static auto as_string(const std::string& path, const star::schema::SchemaValue& value) -> const std::string& {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<std::string>(*scalar)) {
            throw star::excep::RuntimeError("BinaryCodec encode failed, expect string: ", path);
        }
        return std::get<std::string>(*scalar);
    }

    static auto is_conf_value_field(const FieldDescriptor& field) -> bool {
        return field.type_name == "star::schema::SchemaValue";
    }

    static void write_tag(uint32_t field_id, WireType wire_type, std::vector<uint8_t>* out) {
        write_varint((static_cast<uint64_t>(field_id) << 3U) | static_cast<uint8_t>(wire_type), out);
    }

    static void write_varint(uint64_t value, std::vector<uint8_t>* out) {
        while (value >= 0x80) {
            out->push_back(static_cast<uint8_t>(value) | 0x80U);
            value >>= 7U;
        }
        out->push_back(static_cast<uint8_t>(value));
    }

    static auto read_varint(Cursor* cur) -> uint64_t {
        if (cur == nullptr) {
            throw star::excep::RuntimeError("BinaryCodec read_varint failed, cursor is null");
        }
        uint64_t value = 0;
        int shift = 0;
        while (cur->pos < cur->bytes.size()) {
            const auto byte = cur->bytes[cur->pos++];
            value |= static_cast<uint64_t>(byte & 0x7FU) << shift;
            if ((byte & 0x80U) == 0) {
                return value;
            }
            shift += 7;
            if (shift > 63) {
                break;
            }
        }
        throw star::excep::RuntimeError("BinaryCodec read_varint failed, malformed input");
    }

    static void write_fixed32(uint32_t value, std::vector<uint8_t>* out) {
        for (int i = 0; i < 4; ++i) {
            out->push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFFU));
        }
    }

    static void write_fixed64(uint64_t value, std::vector<uint8_t>* out) {
        for (int i = 0; i < 8; ++i) {
            out->push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFFU));
        }
    }

    static auto read_fixed32(Cursor* cur) -> uint32_t {
        ensure_remaining(cur, 4);
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= static_cast<uint32_t>(cur->bytes[cur->pos++]) << (i * 8);
        }
        return value;
    }

    static auto read_fixed64(Cursor* cur) -> uint64_t {
        ensure_remaining(cur, 8);
        uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(cur->bytes[cur->pos++]) << (i * 8);
        }
        return value;
    }

    static auto read_bytes(Cursor* cur) -> std::span<const uint8_t> {
        const auto size = static_cast<size_t>(read_varint(cur));
        ensure_remaining(cur, size);
        auto out = cur->bytes.subspan(cur->pos, size);
        cur->pos += size;
        return out;
    }

    static void ensure_remaining(Cursor* cur, size_t need) {
        if (cur == nullptr || cur->pos + need > cur->bytes.size()) {
            throw star::excep::RuntimeError("BinaryCodec decode failed, unexpected eof");
        }
    }

    static void expect_wire_type(const std::string& path, uint8_t actual, WireType expect) {
        if (actual != static_cast<uint8_t>(expect)) {
            throw star::excep::RuntimeError("BinaryCodec wire type mismatch at: ", path);
        }
    }

    static void skip_unknown(const std::string& path, uint8_t wire_type, Cursor* cur) {
        switch (static_cast<WireType>(wire_type)) {
        case WireType::VARINT:
            static_cast<void>(read_varint(cur));
            return;
        case WireType::FIXED64:
            ensure_remaining(cur, 8);
            cur->pos += 8;
            return;
        case WireType::BYTES: {
            const auto size = static_cast<size_t>(read_varint(cur));
            ensure_remaining(cur, size);
            cur->pos += size;
            return;
        }
        case WireType::FIXED32:
            ensure_remaining(cur, 4);
            cur->pos += 4;
            return;
        }
        throw star::excep::RuntimeError("BinaryCodec skip_unknown failed, unsupported wire type at: ", path);
    }

    static auto consume_unknown_tlv(uint64_t tag, uint32_t field_id, uint8_t wire_type, Cursor* cur) -> std::vector<uint8_t> {
        auto out = std::vector<uint8_t>{};
        write_varint(tag, &out);
        switch (static_cast<WireType>(wire_type)) {
        case WireType::VARINT: {
            const auto value = read_varint(cur);
            write_varint(value, &out);
            return out;
        }
        case WireType::FIXED64: {
            const auto value = read_fixed64(cur);
            write_fixed64(value, &out);
            return out;
        }
        case WireType::BYTES: {
            const auto size = static_cast<size_t>(read_varint(cur));
            write_varint(size, &out);
            ensure_remaining(cur, size);
            out.insert(out.end(), cur->bytes.begin() + static_cast<std::ptrdiff_t>(cur->pos),
                       cur->bytes.begin() + static_cast<std::ptrdiff_t>(cur->pos + size));
            cur->pos += size;
            return out;
        }
        case WireType::FIXED32: {
            const auto value = read_fixed32(cur);
            write_fixed32(value, &out);
            return out;
        }
        }
        throw star::excep::RuntimeError("BinaryCodec consume_unknown_tlv failed, unsupported wire type at field id: ", field_id);
    }
};

}  // namespace star::schema

#endif // __SCHEMA_BINARY_CODEC_H__

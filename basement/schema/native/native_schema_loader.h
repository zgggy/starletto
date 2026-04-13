#ifndef __NATIVE_SCHEMA_LOADER_H__
#define __NATIVE_SCHEMA_LOADER_H__

#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#include "basement/core/exception.h"
#include "basement/schema/native/schema_parser_types.h"
#include "basement/schema/native/schema_syntax_parser.h"
#include "basement/schema/schema_ast.h"
#include "libraries/common/schema/schema_value.h"

namespace star::schema {

class NativeSchemaLoader {
  public:
    auto load_file(const std::string& path) const -> SchemaDescriptor {
        std::ifstream ifs(path);
        if (not ifs.is_open()) {
            throw star::excep::RuntimeError("NativeSchemaLoader load_file failed, cannot open file: ", path);
        }
        std::ostringstream oss;
        oss << ifs.rdbuf();
        const auto text = oss.str();
        if (is_new_schema_syntax(text)) {
            return load_schema_text(text);
        }
        const auto root = star::schema::parse_schema_text(text);
        return load_value(root);
    }

    auto load_value(const star::schema::SchemaValue& root) const -> SchemaDescriptor {
        const auto* root_obj = root.get_object_ptr();
        if (not root_obj) {
            throw star::excep::RuntimeError("NativeSchemaLoader load failed, root is not object");
        }

        SchemaDescriptor out;
        if (const auto* schema_obj = get_object(*root_obj, "schema")) {
            out.type_id.name = get_string(*schema_obj, "name");
            out.type_id.version = get_int64(*schema_obj, "version");
            out.type_id.hash = get_uint64(*schema_obj, "hash");
            out.namespace_name = get_string(*schema_obj, "namespace");
            out.root_name = get_string(*schema_obj, "root");
            out.allow_unknown_fields = get_bool(*schema_obj, "allow_unknown");
        }

        if (const auto* fields_obj = get_object(*root_obj, "fields")) {
            out.fields = parse_fields(*fields_obj);
        }

        if (out.type_id.name.empty()) {
            throw star::excep::RuntimeError("NativeSchemaLoader load failed, schema.name is empty");
        }
        if (out.root_name.empty()) {
            out.root_name = out.type_id.name;
        }
        validate_field_ids(out.fields, out.type_id.name);
        return out;
    }

  private:
    static auto is_new_schema_syntax(std::string_view text) -> bool {
        size_t pos = 0;
        while (pos < text.size()) {
            const auto ch = text[pos];
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                ++pos;
                continue;
            }
            if (ch == '#') {
                while (pos < text.size() && text[pos] != '\n') {
                    ++pos;
                }
                continue;
            }
            if (ch == '/' && pos + 1 < text.size() && text[pos + 1] == '/') {
                pos += 2;
                while (pos < text.size() && text[pos] != '\n') {
                    ++pos;
                }
                continue;
            }
            break;
        }
        if (pos >= text.size()) {
            return false;
        }
        if (text.compare(pos, 6, "schema") != 0) {
            return false;
        }
        pos += 6;
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
            ++pos;
        }
        if (pos >= text.size()) {
            return false;
        }
        return text[pos] != '{';
    }

    auto load_schema_text(std::string_view text) const -> SchemaDescriptor {
        const auto doc = SchemaSyntaxParser(text).parse();
        if (doc.schema_name.empty()) {
            throw star::excep::RuntimeError("NativeSchemaLoader load failed, schema name is empty");
        }

        std::map<std::string, DefBlock> defs;
        for (const auto& def : doc.defs) {
            const auto [_, inserted] = defs.emplace(def.name, def);
            if (not inserted) {
                throw star::excep::RuntimeError("NativeSchemaLoader load failed, duplicate def: ", def.name);
            }
        }

        const auto root_name = doc.root_name.empty() ? doc.schema_name : doc.root_name;
        const auto root_it = defs.find(root_name);
        if (root_it == defs.end()) {
            throw star::excep::RuntimeError("NativeSchemaLoader load failed, missing root def: ", root_name);
        }

        SchemaDescriptor out;
        out.type_id.name = doc.schema_name;
        out.type_id.version = doc.version;
        out.type_id.hash = doc.hash;
        out.namespace_name = doc.namespace_name;
        out.root_name = root_name;
        out.allow_unknown_fields = doc.allow_unknown_fields;
        out.fields = build_fields(root_it->second.fields, defs);
        validate_field_ids(out.fields, out.type_id.name);
        return out;
    }

    static auto build_fields(const std::vector<FieldDef>& fields, const std::map<std::string, DefBlock>& defs)
        -> std::vector<FieldDescriptor> {
        std::vector<FieldDescriptor> out;
        out.reserve(fields.size());
        for (const auto& field : fields) {
            out.emplace_back(build_field(field, defs));
        }
        return out;
    }

    static auto build_field(const FieldDef& field, const std::map<std::string, DefBlock>& defs) -> FieldDescriptor {
        FieldDescriptor out;
        out.id = field.id;
        out.name = field.name;
        out.optional = field.type.optional;
        out.has_default = field.has_default;
        if (field.has_default) {
            out.default_value = field.default_value;
        }
        apply_type_ref(&out, field.type, defs);
        return out;
    }

    static void apply_type_ref(FieldDescriptor* field, const TypeRef& type_ref, const std::map<std::string, DefBlock>& defs) {
        if (field == nullptr) {
            throw star::excep::RuntimeError("NativeSchemaLoader apply_type_ref failed, field is null");
        }
        switch (type_ref.category) {
        case TypeRef::Category::SIMPLE:
            apply_simple_type(field, type_ref.name, defs);
            return;
        case TypeRef::Category::ARRAY:
            field->repeated = true;
            apply_simple_type(field, type_ref.name, defs);
            return;
        case TypeRef::Category::MAP:
            apply_map_type(field, type_ref, defs);
            return;
        }
        throw star::excep::RuntimeError("NativeSchemaLoader apply_type_ref failed, unknown category");
    }

    static void apply_simple_type(FieldDescriptor* field, const std::string& type_name, const std::map<std::string, DefBlock>& defs) {
        if (field == nullptr) {
            throw star::excep::RuntimeError("NativeSchemaLoader apply_simple_type failed, field is null");
        }
        field->type_name = type_name;
        if (type_name == "bool") {
            field->kind = FieldKind::BOOL;
            return;
        }
        if (type_name == "int" || type_name == "int32" || type_name == "int64" || type_name == "uint32" ||
            type_name == "uint64") {
            field->kind = FieldKind::INT64;
            return;
        }
        if (type_name == "float" || type_name == "double" || type_name == "float64") {
            field->kind = FieldKind::FLOAT64;
            return;
        }
        if (type_name == "string") {
            field->kind = FieldKind::STRING;
            return;
        }
        if (type_name == "object") {
            field->kind = FieldKind::OBJECT;
            field->allow_unknown_fields = true;
            return;
        }
        const auto it = defs.find(type_name);
        if (it != defs.end()) {
            field->kind = FieldKind::OBJECT;
            field->children = build_fields(it->second.fields, defs);
            return;
        }
        throw star::excep::RuntimeError("NativeSchemaLoader load failed, unknown type: ", type_name);
    }

    static void apply_map_type(FieldDescriptor* field, const TypeRef& type_ref, const std::map<std::string, DefBlock>& defs) {
        if (field == nullptr) {
            throw star::excep::RuntimeError("NativeSchemaLoader apply_map_type failed, field is null");
        }
        if (type_ref.key_name != "string") {
            throw star::excep::RuntimeError("NativeSchemaLoader load failed, map key must be string");
        }
        field->kind = FieldKind::OBJECT;
        field->type_name = "map";
        field->has_value_kind = true;
        field->value_type_name = type_ref.value_name;

        FieldDescriptor value_field;
        apply_simple_type(&value_field, type_ref.value_name, defs);
        field->value_kind = value_field.kind;
        field->value_children = std::move(value_field.children);
    }

    static auto parse_fields(const star::schema::SchemaObject& obj) -> std::vector<FieldDescriptor> {
        std::vector<FieldDescriptor> out;
        out.reserve(obj.size());
        for (const auto& [field_name, field_value] : obj) {
            const auto* field_obj = field_value.get_object_ptr();
            if (not field_obj) {
                continue;
            }
            FieldDescriptor field;
            field.id = static_cast<uint32_t>(get_uint64(*field_obj, "id"));
            field.name = field_name;
            field.kind = parse_kind(get_string(*field_obj, "kind"));
            field.type_name = get_string(*field_obj, "type");
            const auto value_kind = get_string(*field_obj, "value_kind");
            if (not value_kind.empty()) {
                field.has_value_kind = true;
                field.value_kind = parse_kind(value_kind);
            }
            field.value_type_name = get_string(*field_obj, "value_type");
            field.allow_unknown_fields = get_bool(*field_obj, "allow_unknown");
            field.optional = get_bool(*field_obj, "optional");
            field.repeated = get_bool(*field_obj, "repeated");

            const auto default_it = field_obj->find("default");
            if (default_it != field_obj->end()) {
                field.has_default = true;
                field.default_value = default_it->second;
            }

            if (const auto* child_fields = get_object(*field_obj, "fields")) {
                field.children = parse_fields(*child_fields);
            }
            if (const auto* value_fields = get_object(*field_obj, "value_fields")) {
                field.value_children = parse_fields(*value_fields);
            }
            out.emplace_back(std::move(field));
        }
        return out;
    }

    static void validate_field_ids(const std::vector<FieldDescriptor>& fields, const std::string& scope) {
        std::unordered_set<uint32_t> ids;
        for (const auto& field : fields) {
            if (field.id == 0) {
                throw star::excep::RuntimeError("NativeSchemaLoader load failed, missing field id in scope: ", scope, ", field=", field.name);
            }
            if (not ids.emplace(field.id).second) {
                throw star::excep::RuntimeError("NativeSchemaLoader load failed, duplicate field id in scope: ", scope, ", id=", field.id);
            }
            if (not field.children.empty()) {
                validate_field_ids(field.children, scope + "." + field.name);
            }
            if (not field.value_children.empty()) {
                validate_field_ids(field.value_children, scope + "." + field.name + ".$value");
            }
        }
    }

    static auto parse_kind(const std::string& value) -> FieldKind {
        if (value == "bool") {
            return FieldKind::BOOL;
        }
        if (value == "int64") {
            return FieldKind::INT64;
        }
        if (value == "float64") {
            return FieldKind::FLOAT64;
        }
        if (value == "object") {
            return FieldKind::OBJECT;
        }
        if (value == "array") {
            return FieldKind::ARRAY;
        }
        return FieldKind::STRING;
    }

    static auto get_object(const star::schema::SchemaObject& obj, const std::string& key)
        -> const star::schema::SchemaObject* {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return nullptr;
        }
        return it->second.get_object_ptr();
    }

    static auto get_string(const star::schema::SchemaObject& obj, const std::string& key) -> std::string {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return {};
        }
        const auto* scalar = it->second.get_scalar_ptr();
        if (not scalar) {
            return {};
        }
        if (std::holds_alternative<std::string>(*scalar)) {
            return std::get<std::string>(*scalar);
        }
        return {};
    }

    static auto get_int64(const star::schema::SchemaObject& obj, const std::string& key) -> int64_t {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return 0;
        }
        const auto* scalar = it->second.get_scalar_ptr();
        if (not scalar) {
            return 0;
        }
        if (std::holds_alternative<int64_t>(*scalar)) {
            return std::get<int64_t>(*scalar);
        }
        if (std::holds_alternative<double>(*scalar)) {
            return static_cast<int64_t>(std::get<double>(*scalar));
        }
        return 0;
    }

    static auto get_uint64(const star::schema::SchemaObject& obj, const std::string& key) -> uint64_t {
        const auto v = get_int64(obj, key);
        if (v <= 0) {
            return 0;
        }
        return static_cast<uint64_t>(v);
    }

    static auto get_bool(const star::schema::SchemaObject& obj, const std::string& key) -> bool {
        const auto it = obj.find(key);
        if (it == obj.end()) {
            return false;
        }
        const auto* scalar = it->second.get_scalar_ptr();
        if (not scalar) {
            return false;
        }
        if (std::holds_alternative<bool>(*scalar)) {
            return std::get<bool>(*scalar);
        }
        return false;
    }
};

}  // namespace star::schema

#endif // __NATIVE_SCHEMA_LOADER_H__

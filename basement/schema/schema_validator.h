#ifndef __SCHEMA_VALIDATOR_H__
#define __SCHEMA_VALIDATOR_H__

#include "basement/core/exception.h"
#include "basement/schema/schema_ast.h"

#include <string>

namespace star::schema {

class SchemaValidator {
public:
    static void validate(const SchemaDescriptor& schema, const star::schema::SchemaValue& value) {
        validate_object_fields(schema.root_name, schema.fields, schema.allow_unknown_fields, value);
    }

    static void validate(const SchemaDescriptor& schema, const star::schema::SchemaValue* value) {
        if (value == nullptr) {
            throw star::excep::RuntimeError("SchemaValidator validate failed, value is null");
        }
        validate(schema, *value);
    }

    static void apply_defaults(const SchemaDescriptor& schema, star::schema::SchemaValue* value) {
        if (value == nullptr) {
            throw star::excep::RuntimeError("SchemaValidator apply_defaults failed, value is null");
        }
        apply_object_defaults(schema.root_name, schema.fields, value);
    }

private:
    static void validate_object_fields(
        const std::string& path,
        const std::vector<FieldDescriptor>& fields,
        bool allow_unknown_fields,
        const star::schema::SchemaValue& value) {
        const auto* obj = ensure_object(path, value);
        for (const auto& field : fields) {
            auto it = obj->find(field.name);
            if (it == obj->end()) {
                if (field.optional || (field.has_default && !field.explicit_required)) {
                    continue;
                } else {
                    throw star::excep::RuntimeError("SchemaValidator missing required field: ", join_path(path, field.name));
                }
            }
            validate_field(join_path(path, field.name), field, it->second);
        }
        if (allow_unknown_fields) {
            return;
        }
        for (const auto& [key, _] : *obj) {
            if (find_field(fields, key) == nullptr) {
                throw star::excep::RuntimeError("SchemaValidator unknown field: ", join_path(path, key));
            }
        }
    }

    static void apply_object_defaults(const std::string& path, const std::vector<FieldDescriptor>& fields, star::schema::SchemaValue* value) {
        auto* obj = ensure_object(path, value);
        for (const auto& field : fields) {
            auto it = obj->find(field.name);
            if (it == obj->end()) {
                if (field.has_default && !field.explicit_required) {
                    obj->emplace(field.name, field.default_value);
                    it = obj->find(field.name);
                } else {
                    continue;
                }
            }
            apply_field_defaults(join_path(path, field.name), field, &it->second);
        }
    }

    static void apply_field_defaults(const std::string& path, const FieldDescriptor& field, star::schema::SchemaValue* value) {
        if (value == nullptr) {
            throw star::excep::RuntimeError("SchemaValidator apply_defaults failed, value is null at: ", path);
        }
        if (field.repeated) {
            auto* arr = ensure_array(path, value);
            for (size_t i = 0; i < arr->size(); ++i) {
                auto item_field = field;
                item_field.repeated = false;
                apply_field_defaults(path + "[" + std::to_string(i) + "]", item_field, &(*arr)[i]);
            }
            return;
        }
        if (field.kind == FieldKind::OBJECT && !field.has_value_kind) {
            apply_object_defaults(path, field.children, value);
        }
    }

    static void validate_field(const std::string& path, const FieldDescriptor& field, const star::schema::SchemaValue& value) {
        if (field.repeated) {
            const auto* arr = ensure_array(path, value);
            for (size_t i = 0; i < arr->size(); ++i) {
                auto item_field = field;
                item_field.repeated = false;
                validate_field(path + "[" + std::to_string(i) + "]", item_field, (*arr)[i]);
            }
            return;
        }

        switch (field.kind) {
        case FieldKind::BOOL:
            ensure_bool(path, value);
            return;
        case FieldKind::INT64:
            ensure_int64(path, value);
            return;
        case FieldKind::FLOAT64:
            ensure_float64(path, value);
            return;
        case FieldKind::STRING:
            ensure_string(path, value);
            return;
        case FieldKind::OBJECT:
            if (field.has_value_kind) {
                validate_object_values(path, field, value);
                return;
            }
            validate_object_fields(path, field.children, field.allow_unknown_fields, value);
            return;
        }
    }

    static void validate_object_values(const std::string& path, const FieldDescriptor& field, const star::schema::SchemaValue& value) {
        const auto* obj = ensure_object(path, value);
        auto value_field = FieldDescriptor{};
        value_field.kind = field.value_kind;
        value_field.type_name = field.value_type_name;
        value_field.children = field.value_children;
        for (auto& [key, child] : *obj) {
            validate_field(join_path(path, key), value_field, child);
        }
    }

    static auto find_field(const std::vector<FieldDescriptor>& fields, const std::string& name) -> const FieldDescriptor* {
        for (const auto& field : fields) {
            if (field.name == name) {
                return &field;
            }
        }
        return nullptr;
    }

    static auto ensure_object(const std::string& path, star::schema::SchemaValue* value) -> star::schema::SchemaObject* {
        if (value == nullptr || not value->is_object()) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect object at: ", path);
        }
        return &value->as_object();
    }

    static auto ensure_object(const std::string& path, const star::schema::SchemaValue& value) -> const star::schema::SchemaObject* {
        if (not value.is_object()) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect object at: ", path);
        }
        return &value.as_object();
    }

    static auto ensure_array(const std::string& path, star::schema::SchemaValue* value) -> star::schema::SchemaArray* {
        if (value == nullptr || not value->is_array()) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect array at: ", path);
        }
        return &value->as_array();
    }

    static auto ensure_array(const std::string& path, const star::schema::SchemaValue& value) -> const star::schema::SchemaArray* {
        if (not value.is_array()) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect array at: ", path);
        }
        return &value.as_array();
    }

    static void ensure_bool(const std::string& path, const star::schema::SchemaValue& value) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<bool>(*scalar)) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect bool at: ", path);
        }
    }

    static void ensure_int64(const std::string& path, const star::schema::SchemaValue& value) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<int64_t>(*scalar)) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect int64 at: ", path);
        }
    }

    static void ensure_float64(const std::string& path, const star::schema::SchemaValue& value) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect float64 at: ", path);
        }
        if (std::holds_alternative<double>(*scalar) || std::holds_alternative<int64_t>(*scalar)) {
            return;
        }
        throw star::excep::RuntimeError("SchemaValidator type mismatch, expect float64 at: ", path);
    }

    static void ensure_string(const std::string& path, const star::schema::SchemaValue& value) {
        const auto* scalar = value.get_scalar_ptr();
        if (scalar == nullptr || not std::holds_alternative<std::string>(*scalar)) {
            throw star::excep::RuntimeError("SchemaValidator type mismatch, expect string at: ", path);
        }
    }

    static auto join_path(const std::string& lhs, const std::string& rhs) -> std::string {
        if (lhs.empty()) {
            return rhs;
        }
        if (rhs.empty()) {
            return lhs;
        }
        return lhs + "." + rhs;
    }
};

}  // namespace star::schema

#endif // __SCHEMA_VALIDATOR_H__

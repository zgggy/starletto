#ifndef __SCHEMA_REGISTRY_H__
#define __SCHEMA_REGISTRY_H__

#include "basement/core/exception.h"
#include "basement/schema/schema_ast.h"

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace star::schema {

class SchemaRegistry {
public:
    void register_schema(SchemaDescriptor schema) {
        if (schema.type_id.name.empty()) {
            throw star::excep::RuntimeError("SchemaRegistry register_schema failed, empty schema name");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const auto key = build_key(schema.type_id);
        schemas_by_key_[key] = schema;
        schemas_by_name_[schema.type_id.name] = key;
    }

    auto find_by_type_id(const SchemaTypeId& type_id) const -> std::optional<SchemaDescriptor> {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = schemas_by_key_.find(build_key(type_id));
        if (it == schemas_by_key_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    auto find_by_name(const std::string& name) const -> std::optional<SchemaDescriptor> {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto name_it = schemas_by_name_.find(name);
        if (name_it == schemas_by_name_.end()) {
            return std::nullopt;
        }
        const auto schema_it = schemas_by_key_.find(name_it->second);
        if (schema_it == schemas_by_key_.end()) {
            return std::nullopt;
        }
        return schema_it->second;
    }

private:
    static auto build_key(const SchemaTypeId& type_id) -> std::string {
        return type_id.name + "#" + std::to_string(type_id.version) + "#" + std::to_string(type_id.hash);
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SchemaDescriptor> schemas_by_key_;
    std::unordered_map<std::string, std::string> schemas_by_name_;
};

}  // namespace star::schema

#endif // __SCHEMA_REGISTRY_H__

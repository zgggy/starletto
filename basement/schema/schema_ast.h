#ifndef __SCHEMA_AST_H__
#define __SCHEMA_AST_H__

#include "basement/schema/type_id.h"
#include "libraries/common/schema/schema_value.h"

#include <cstdint>
#include <string>
#include <vector>

namespace star::schema {

enum class FieldKind : uint8_t {
    BOOL = 0,
    INT64 = 1,
    FLOAT64 = 2,
    STRING = 3,
    OBJECT = 4,
};

struct FieldDescriptor {
    uint32_t id{0};
    std::string name;
    FieldKind kind{FieldKind::STRING};
    std::string type_name;
    bool has_value_kind{false};
    FieldKind value_kind{FieldKind::STRING};
    std::string value_type_name;
    bool allow_unknown_fields{false};
    bool optional{false};
    bool explicit_required{false};
    bool repeated{false};
    bool has_default{false};
    star::schema::SchemaValue default_value{};
    std::vector<FieldDescriptor> children;
    std::vector<FieldDescriptor> value_children;
};

struct SchemaDescriptor {
    SchemaTypeId type_id{};
    std::string namespace_name;
    std::string root_name;
    bool allow_unknown_fields{false};
    std::vector<FieldDescriptor> fields;
};

}  // namespace star::schema

#endif // __SCHEMA_AST_H__

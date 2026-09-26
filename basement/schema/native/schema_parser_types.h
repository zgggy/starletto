#ifndef __SCHEMA_PARSER_TYPES_H__
#define __SCHEMA_PARSER_TYPES_H__

#include <cstdint>
#include <string>
#include <vector>

#include "basement/schema/schema_value.h"

namespace star::schema {

struct TypeRef {
    enum class Category : uint8_t {
        SIMPLE = 0,
        ARRAY = 1,
        MAP = 2,
    };

    Category category{Category::SIMPLE};
    std::string name;
    std::string key_name;
    std::string value_name;
    bool optional{false};
    bool explicit_required{false};
};

struct FieldDef {
    uint32_t id{0};
    std::string name;
    TypeRef type;
    bool has_default{false};
    star::schema::SchemaValue default_value{};
};

struct DefBlock {
    std::string name;
    std::vector<FieldDef> fields;
};

struct SchemaSyntaxDocument {
    std::string schema_name;
    std::string root_name;
    std::string namespace_name;
    int64_t version{0};
    uint64_t hash{0};
    bool allow_unknown_fields{false};
    std::vector<DefBlock> defs;
};

}  // namespace star::schema

#endif  // __SCHEMA_PARSER_TYPES_H__

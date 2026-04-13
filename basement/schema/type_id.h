#ifndef __SCHEMA_TYPE_ID_H__
#define __SCHEMA_TYPE_ID_H__

#include <cstdint>
#include <string>

namespace star::schema {

struct SchemaTypeId {
    std::string name;
    int64_t version{0};
    uint64_t hash{0};

    auto empty() const -> bool {
        return name.empty() && version == 0 && hash == 0;
    }
};

inline auto operator==(const SchemaTypeId& lhs, const SchemaTypeId& rhs) -> bool {
    return lhs.name == rhs.name && lhs.version == rhs.version && lhs.hash == rhs.hash;
}

inline auto operator!=(const SchemaTypeId& lhs, const SchemaTypeId& rhs) -> bool {
    return !(lhs == rhs);
}

}  // namespace star::schema

#endif // __SCHEMA_TYPE_ID_H__

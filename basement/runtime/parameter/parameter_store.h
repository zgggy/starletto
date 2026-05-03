#ifndef __PARAMETER_STORE_H__
#define __PARAMETER_STORE_H__

#include "libraries/common/schema/schema_value.h"

#include <string>
#include <vector>

namespace star::runtime {

class ParameterStore {
public:
    virtual ~ParameterStore() = default;

    virtual auto get(const std::string& key) const -> star::schema::SchemaValue = 0;
    virtual void set(const std::string& key, const star::schema::SchemaValue& value) = 0;
    virtual auto list(const std::string& prefix) const -> std::vector<std::string> = 0;
    virtual void flush() = 0;
};

}  // namespace star::runtime

#endif // __PARAMETER_STORE_H__

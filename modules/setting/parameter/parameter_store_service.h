#ifndef __PARAMETER_STORE_SERVICE_H__
#define __PARAMETER_STORE_SERVICE_H__

#include "basement/runtime/parameter/parameter_store.h"

#include <memory>
#include <string>
#include <vector>

namespace star::setting {

class ParameterStoreService {
public:
    explicit ParameterStoreService(std::shared_ptr<star::runtime::ParameterStore> parameter_store)
        : parameter_store_(std::move(parameter_store)) {}

    auto get(const std::string& key) const -> star::schema::SchemaValue {
        return parameter_store_->get(key);
    }

    void set(const std::string& key, const star::schema::SchemaValue& value) {
        parameter_store_->set(key, value);
    }

    auto list(const std::string& prefix) const -> std::vector<std::string> {
        return parameter_store_->list(prefix);
    }

private:
    std::shared_ptr<star::runtime::ParameterStore> parameter_store_;
};

}  // namespace star::setting

#endif // __PARAMETER_STORE_SERVICE_H__

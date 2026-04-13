#ifndef __NATIVE_PARAMETER_STORE_H__
#define __NATIVE_PARAMETER_STORE_H__

#include "basement/transport/api/parameter_store.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace star::transport {

class NativeParameterStore final : public ParameterStore {
public:
    NativeParameterStore() = default;
    ~NativeParameterStore() override = default;

    auto get(const std::string& key) const -> star::schema::SchemaValue override {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = values_.find(key);
        if (it == values_.end()) {
            return star::schema::SchemaValue(nullptr);
        }
        return it->second;
    }

    void set(const std::string& key, const star::schema::SchemaValue& value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        values_[key] = value;
    }

    auto list(const std::string& prefix) const -> std::vector<std::string> override {
        std::vector<std::string> out;
        std::lock_guard<std::mutex> lock(mutex_);
        out.reserve(values_.size());
        for (const auto& [k, _] : values_) {
            if (prefix.empty()) {
                out.push_back(k);
                continue;
            }
            if (k.rfind(prefix, 0) == 0) {
                out.push_back(k);
            }
        }
        return out;
    }

    void flush() override {}

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, star::schema::SchemaValue> values_;
};

}  // namespace star::transport

#endif // __NATIVE_PARAMETER_STORE_H__

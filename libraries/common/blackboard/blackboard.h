#ifndef __COMMON_BLACKBOARD_H__
#define __COMMON_BLACKBOARD_H__

#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "basement/core/common_types.h"
#include "basement/core/logger/logger.h"

namespace star::common {

class Blackboard {
  public:
    struct Entry {
        TypedPtr value;
        TypedPtr default_value;
        int64_t last_update_frame{-1};
        double last_update_time{0.0};
        std::string source;
        uint64_t version{0};
    };

    Blackboard() {
        add("sequence_number", static_cast<size_t>(0));
        add("status", static_cast<char>(0));
    }
    virtual ~Blackboard() = default;

    void clear() {
        std::unique_lock lock(mutex_);
        data_.clear();
    }

    void reset() {
        std::unique_lock lock(mutex_);
        for (auto& [name, ent] : data_) {
            ent.value = ent.default_value;
            ent.last_update_frame = -1;
            ent.version = 0;
        }
    }

    template <typename _T>
    void add(const std::string& name, const _T& data, const std::string& source = "") {
        std::unique_lock lock(mutex_);
        auto ptr = std::make_shared<_T>(data);
        auto tp = TypedPtr::from(ptr);
        data_.insert_or_assign(name, Entry{tp, tp, -1, 0.0, source, 0});
    }

    template <typename _T>
    void add(const std::string& name, const _T& data, const _T& default_value, const std::string& source = "") {
        std::unique_lock lock(mutex_);
        auto val_ptr = std::make_shared<_T>(data);
        auto def_ptr = std::make_shared<_T>(default_value);
        data_.insert_or_assign(name, Entry{TypedPtr::from(val_ptr), TypedPtr::from(def_ptr), -1, 0.0, source, 0});
    }

    template <typename _T>
    void set(const std::string& name, const _T& data, const std::string& source = "") {
        std::unique_lock lock(mutex_);
        auto it = data_.find(name);
        auto ptr = std::make_shared<_T>(data);
        auto tp = TypedPtr::from(ptr);
        if (it == data_.end()) {
            data_.insert_or_assign(name, Entry{tp, tp, -1, 0.0, source, 1});
        } else {
            it->second.value = tp;
            it->second.last_update_frame = static_cast<int64_t>(frame_number_);
            it->second.source = source;
            it->second.version++;
        }
    }

    template <typename _T>
    auto get(const std::string& name) const -> _T {
        std::shared_lock lock(mutex_);
        auto it = data_.find(name);
        if (it == data_.end()) {
            logE << "Blackboard get failed, key not found: " << name;
            throw star::excep::RuntimeError("Blackboard get failed, key not found: ", name);
        }
        auto p = it->second.value.as<_T>();
        if (not p) {
            logE << "Blackboard get failed, type mismatch for key: " << name;
            throw star::excep::RuntimeError("Blackboard get failed, type mismatch for key: ", name);
        }
        return *p;
    }

    template <typename _T>
    auto get_or(const std::string& name, _T default_value) const -> _T {
        std::shared_lock lock(mutex_);
        auto it = data_.find(name);
        if (it == data_.end()) {
            return default_value;
        }
        auto p = it->second.value.as<_T>();
        if (not p) {
            return default_value;
        }
        return *p;
    }

    template <typename _T>
    auto get_ptr(const std::string& name) const -> const _T* {
        std::shared_lock lock(mutex_);
        auto it = data_.find(name);
        if (it == data_.end()) {
            return nullptr;
        }
        auto p = it->second.value.as<_T>();
        return p.get();
    }

    template <typename _T>
    auto get_ptr(const std::string& name) -> _T* {
        std::shared_lock lock(mutex_);
        auto it = data_.find(name);
        if (it == data_.end()) {
            return nullptr;
        }
        auto p = it->second.value.as<_T>();
        return p.get();
    }

    auto has(const std::string& name) const -> bool {
        std::shared_lock lock(mutex_);
        return data_.find(name) != data_.end();
    }

    void update() {
        std::unique_lock lock(mutex_);
        ++frame_number_;
    }

    auto frame_number() const -> size_t {
        std::shared_lock lock(mutex_);
        return frame_number_;
    }

  private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Entry> data_;
    size_t frame_number_{0};
};

}  // namespace star::common

#endif  // __COMMON_BLACKBOARD_H__

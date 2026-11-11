#ifndef __SCHEMA_REGISTRY_H__
#define __SCHEMA_REGISTRY_H__

#include "basement/core/exception.h"
#include "basement/schema/schema_ast.h"

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace star::schema {

// 一次 schema 查找的结果。必须区分「没找到」与「找到了但内容不一致」：
// 前者可能是类型尚未加载，后者是同一个 name+version 被不同内容注册（版本冲突），
// 两者的处置完全不同（P2 阶段三的解码策略按这个状态分支）。
enum class SchemaMatchStatus : uint8_t {
    kExact = 0,         // name+version 命中，且 hash 一致
    kHashUnknown = 1,   // name+version 命中，但一方没有 hash（0），无法确认内容
    kHashMismatch = 2,  // name+version 命中，但内容 hash 不同
    kNotFound = 3,
};

struct SchemaLookupResult {
    SchemaMatchStatus status{SchemaMatchStatus::kNotFound};
    std::optional<SchemaDescriptor> schema{};
};

// 多版本共存：key = name + version（不含 hash）。
// hash 是内容指纹，不参与身份——同一个 name+version 的两次注册若内容不同，
// 那是错误（同版本不同内容），必须报出来，而不是各存一份。
// 各版本的解码策略由上层（通道/录制）决定，registry 只负责存储与查找。
class SchemaRegistry {
public:
    void register_schema(SchemaDescriptor schema) {
        if (schema.type_id.name.empty()) {
            throw star::excep::RuntimeError("SchemaRegistry register_schema failed, empty schema name");
        }
        if (schema.type_id.version < 0) {
            throw star::excep::RuntimeError("SchemaRegistry register_schema failed, negative version: ",
                                            schema.type_id.name,
                                            ", version=",
                                            schema.type_id.version);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const auto key = build_key(schema.type_id.name, schema.type_id.version);
        const auto key_it = schemas_by_key_.find(key);
        if (key_it != schemas_by_key_.end()) {
            const auto& existing = key_it->second;
            if (not same_content(existing, schema)) {
                throw star::excep::RuntimeError(
                    "SchemaRegistry register_schema failed, schema registered twice with different content: ",
                    schema.type_id.name,
                    ", version=",
                    schema.type_id.version,
                    ", existing_hash=",
                    existing.type_id.hash,
                    ", new_hash=",
                    schema.type_id.hash);
            }
            return;
        }

        auto versions_it = versions_by_name_.find(schema.type_id.name);
        if (versions_it == versions_by_name_.end()) {
            versions_by_name_.emplace(schema.type_id.name, std::vector<int64_t>{schema.type_id.version});
        } else {
            auto& versions = versions_it->second;
            const auto pos = std::lower_bound(versions.begin(), versions.end(), schema.type_id.version);
            if (pos == versions.end() or *pos != schema.type_id.version) {
                versions.insert(pos, schema.type_id.version);
            }
        }
        schemas_by_key_.emplace(key, std::move(schema));
    }

    // 按 name+version 精确查找（不看 hash）。
    auto find_by_name_and_version(const std::string& name, int64_t version) const -> std::optional<SchemaDescriptor> {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = schemas_by_key_.find(build_key(name, version));
        if (it == schemas_by_key_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // 查找某个 name 下版本号最高的 schema（未声明多版本时的既有语义）。
    auto find_latest(const std::string& name) const -> std::optional<SchemaDescriptor> {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto versions_it = versions_by_name_.find(name);
        if (versions_it == versions_by_name_.end() or versions_it->second.empty()) {
            return std::nullopt;
        }
        const auto it = schemas_by_key_.find(build_key(name, versions_it->second.back()));
        if (it == schemas_by_key_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    // 某个 name 已注册的全部版本，升序。
    auto versions(const std::string& name) const -> std::vector<int64_t> {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = versions_by_name_.find(name);
        if (it == versions_by_name_.end()) {
            return {};
        }
        return it->second;
    }

    // 带内容校验的查找。hash=0 表示「不知道内容」，视为未知而非不匹配。
    auto lookup(const SchemaTypeId& type_id) const -> SchemaLookupResult {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = schemas_by_key_.find(build_key(type_id.name, type_id.version));
        if (it == schemas_by_key_.end()) {
            return SchemaLookupResult{};
        }
        const auto& schema = it->second;
        if (type_id.hash == 0 or schema.type_id.hash == 0) {
            return SchemaLookupResult{SchemaMatchStatus::kHashUnknown, schema};
        }
        if (type_id.hash != schema.type_id.hash) {
            return SchemaLookupResult{SchemaMatchStatus::kHashMismatch, schema};
        }
        return SchemaLookupResult{SchemaMatchStatus::kExact, schema};
    }

    // 只读便利接口：精确或「hash 未知」时返回 schema，内容冲突/不存在时返回空。
    auto find_by_type_id(const SchemaTypeId& type_id) const -> std::optional<SchemaDescriptor> {
        auto result = lookup(type_id);
        if (result.status == SchemaMatchStatus::kExact or result.status == SchemaMatchStatus::kHashUnknown) {
            return std::move(result.schema);
        }
        return std::nullopt;
    }

    auto size() const -> size_t {
        std::lock_guard<std::mutex> lock(mutex_);
        return schemas_by_key_.size();
    }

private:
    static auto build_key(const std::string& name, int64_t version) -> std::string {
        return name + "#" + std::to_string(version);
    }

    static auto fields_equal(const std::vector<FieldDescriptor>& lhs, const std::vector<FieldDescriptor>& rhs) -> bool {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (size_t index = 0; index < lhs.size(); ++index) {
            const auto& a = lhs[index];
            const auto& b = rhs[index];
            if (a.id != b.id or a.name != b.name or a.kind != b.kind or a.type_name != b.type_name or
                a.has_value_kind != b.has_value_kind or a.value_kind != b.value_kind or
                a.value_type_name != b.value_type_name or a.allow_unknown_fields != b.allow_unknown_fields or
                a.optional != b.optional or a.explicit_required != b.explicit_required or a.repeated != b.repeated or
                a.has_default != b.has_default) {
                return false;
            }
            if (a.has_default != b.has_default) {
                return false;
            }
            if (a.has_default) {
                if (a.default_value.is_scalar() != b.default_value.is_scalar()) {
                    return false;
                }
                if (a.default_value.is_scalar() and a.default_value.as_scalar() != b.default_value.as_scalar()) {
                    return false;
                }
            }
            if (not fields_equal(a.children, b.children) or not fields_equal(a.value_children, b.value_children)) {
                return false;
            }
        }
        return true;
    }

    static auto descriptor_equal(const SchemaDescriptor& lhs, const SchemaDescriptor& rhs) -> bool {
        return lhs.type_id.name == rhs.type_id.name and lhs.type_id.version == rhs.type_id.version and
               lhs.namespace_name == rhs.namespace_name and lhs.root_name == rhs.root_name and
               lhs.allow_unknown_fields == rhs.allow_unknown_fields and fields_equal(lhs.fields, rhs.fields);
    }

    // 同 name+version 的重复注册是否等价。
    // 双方都有 hash 时按 hash 判定；任何一方 hash=0（手工构造的 descriptor）时退回结构比较。
    static auto same_content(const SchemaDescriptor& lhs, const SchemaDescriptor& rhs) -> bool {
        if (lhs.type_id.hash != 0 and rhs.type_id.hash != 0) {
            return lhs.type_id.hash == rhs.type_id.hash;
        }
        return descriptor_equal(lhs, rhs);
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SchemaDescriptor> schemas_by_key_;
    std::unordered_map<std::string, std::vector<int64_t>> versions_by_name_;
};

}  // namespace star::schema

#endif // __SCHEMA_REGISTRY_H__

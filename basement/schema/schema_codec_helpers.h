#ifndef __BASEMENT_SCHEMA_CODEC_HELPERS_H__
#define __BASEMENT_SCHEMA_CODEC_HELPERS_H__

#include "basement/schema/binary_codec.h"
#include "basement/schema/schema_ast.h"
#include "libraries/common/schema/schema_value.h"

#include <concepts>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace star::schema {

template <typename _ValueType>
concept SchemaStructType = requires(const SchemaValue& value, const _ValueType& item) {
    { _ValueType::from_value(value) } -> std::same_as<_ValueType>;
    { item.to_value() } -> std::same_as<SchemaValue>;
};

template <typename _ValueType>
struct IsStdVector : std::false_type {};

template <typename _ItemType, typename _Allocator>
struct IsStdVector<std::vector<_ItemType, _Allocator>> : std::true_type {
    using Item = _ItemType;
};

template <typename _ValueType>
struct IsStdMap : std::false_type {};

template <typename _MappedType, typename _Compare, typename _Allocator>
struct IsStdMap<std::map<std::string, _MappedType, _Compare, _Allocator>> : std::true_type {
    using Item = _MappedType;
};

template <typename _ValueType>
auto schema_read(const SchemaValue& value) -> _ValueType {
    if constexpr (IsStdVector<_ValueType>::value) {
        using _Item = typename IsStdVector<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& arr = value.as_array();
        out.reserve(arr.size());
        for (const auto& item : arr) {
            out.emplace_back(schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (IsStdMap<_ValueType>::value) {
        using _Item = typename IsStdMap<_ValueType>::Item;
        auto out = _ValueType{};
        const auto& obj = value.as_object();
        for (const auto& [key, item] : obj) {
            out.emplace(key, schema_read<_Item>(item));
        }
        return out;
    } else if constexpr (std::is_same_v<_ValueType, std::string>) {
        return value.as<std::string>();
    } else if constexpr (std::is_same_v<_ValueType, SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, bool>) {
        return value.as<bool>();
    } else if constexpr (std::is_same_v<_ValueType, int64_t>) {
        return value.as<int64_t>();
    } else if constexpr (std::is_same_v<_ValueType, double>) {
        return value.as<double>();
    } else if constexpr (std::is_same_v<_ValueType, SchemaObject>) {
        return value.as_object();
    } else if constexpr (SchemaStructType<_ValueType>) {
        return _ValueType::from_value(value);
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_read type");
    }
}

template <typename _ValueType>
auto schema_write(const _ValueType& value) -> SchemaValue {
    if constexpr (IsStdVector<_ValueType>::value) {
        auto out = SchemaArray{};
        out.reserve(value.size());
        for (const auto& item : value) {
            out.emplace_back(schema_write(item));
        }
        return SchemaValue(std::move(out));
    } else if constexpr (IsStdMap<_ValueType>::value) {
        auto out = SchemaObject{};
        for (const auto& [key, item] : value) {
            out.emplace(key, schema_write(item));
        }
        return SchemaValue(std::move(out));
    } else if constexpr (std::is_same_v<_ValueType, SchemaValue>) {
        return value;
    } else if constexpr (std::is_same_v<_ValueType, std::string> || std::is_same_v<_ValueType, bool> ||
                  std::is_same_v<_ValueType, int64_t> || std::is_same_v<_ValueType, double>) {
        return schema_value_from(value);
    } else if constexpr (std::is_same_v<_ValueType, SchemaObject>) {
        return SchemaValue(value);
    } else if constexpr (SchemaStructType<_ValueType>) {
        return value.to_value();
    } else {
        static_assert(std::is_same_v<_ValueType, void>, "unsupported schema_write type");
    }
}

}  // namespace star::schema

#endif  // __BASEMENT_SCHEMA_CODEC_HELPERS_H__

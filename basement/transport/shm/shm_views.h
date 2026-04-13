#ifndef __SHM_VIEWS_H__
#define __SHM_VIEWS_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "libraries/common/schema/schema_value.h"

namespace star::transport {

struct ShmTextView {
    std::shared_ptr<void> mapping;
    const char* data{nullptr};
    size_t size{0};
    std::string channel_name;
    star::schema::SchemaTypeId schema_type_id;

    auto text() const -> std::string_view {
        if (data == nullptr || size == 0) {
            return {};
        }
        return std::string_view(data, size);
    }
};

struct ShmBinaryView {
    std::shared_ptr<void> mapping;
    const uint8_t* data{nullptr};
    size_t size{0};
    std::string channel_name;
    star::schema::SchemaTypeId schema_type_id;

    auto bytes() const -> std::span<const uint8_t> {
        if (data == nullptr || size == 0) {
            return {};
        }
        return std::span<const uint8_t>(data, size);
    }
};

}  // namespace star::transport

#endif  // __SHM_VIEWS_H__

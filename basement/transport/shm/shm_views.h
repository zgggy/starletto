#ifndef __SHM_VIEWS_H__
#define __SHM_VIEWS_H__

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "basement/transport/common/fixed_layout_codec.h"
#include "basement/schema/type_id.h"

namespace star::transport {

struct ShmSlotView {
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

    auto as_fixed_layout() const -> FixedLayoutView {
        if (data == nullptr || size == 0) {
            return FixedLayoutView{};
        }
        return FixedLayoutView(data, size);
    }

    auto text() const -> std::string_view {
        if (data == nullptr || size == 0) {
            return {};
        }
        return std::string_view(reinterpret_cast<const char*>(data), size);
    }
};

}  // namespace star::transport

#endif  // __SHM_VIEWS_H__

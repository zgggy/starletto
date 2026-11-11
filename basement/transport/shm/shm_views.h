#ifndef __SHM_VIEWS_H__
#define __SHM_VIEWS_H__

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "basement/schema/fixed_layout_codec.h"
#include "basement/schema/type_id.h"

namespace star::transport {

// shm 零拷贝只读视图。
// 槽位会被写方按 seq % slot_count 覆写，因此视图必须携带**该槽的代际信息**，
// 读取前后各复核一次（seqlock）；否则延迟解码会读到被覆写的撕裂数据。
// 注意：不能用「全局 seq 推进量」推断——写方在 memcpy 期间 seq 尚未推进，
// 彼时本槽正在被覆盖，任何基于 seq 的推断都会误判为稳定。
struct ShmSlotView {
    std::shared_ptr<void> mapping;
    const uint8_t* data{nullptr};
    size_t size{0};
    std::string channel_name;
    star::schema::SchemaTypeId schema_type_id;
    const std::atomic<uint32_t>* gen_addr{nullptr};
    uint32_t observed_gen{0};

    auto stable() const -> bool {
        if (gen_addr == nullptr) {
            return true;
        }
        const auto gen = gen_addr->load(std::memory_order_acquire);
        // 奇数 = 写方正在覆写本槽；与建视图时代际不同 = 已被覆写
        return gen == observed_gen && (gen & 1u) == 0u;
    }

    auto bytes() const -> std::span<const uint8_t> {
        if (data == nullptr || size == 0 || not stable()) {
            return {};
        }
        return std::span<const uint8_t>(data, size);
    }

    auto as_fixed_layout() const -> star::schema::FixedLayoutView {
        if (data == nullptr || size == 0 || not stable()) {
            return star::schema::FixedLayoutView{};
        }
        return star::schema::FixedLayoutView(data, size);
    }

    auto text() const -> std::string_view {
        if (data == nullptr || size == 0 || not stable()) {
            return {};
        }
        return std::string_view(reinterpret_cast<const char*>(data), size);
    }
};

}  // namespace star::transport

#endif  // __SHM_VIEWS_H__

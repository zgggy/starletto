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
// 槽位会被写方按 seq % slot_count 覆写，因此视图必须携带建视图时的代际信息，
// 读取前复核是否仍稳定（seqlock）；否则延迟解码会读到被覆写的撕裂数据。
struct ShmSlotView {
    std::shared_ptr<void> mapping;
    const uint8_t* data{nullptr};
    size_t size{0};
    std::string channel_name;
    star::schema::SchemaTypeId schema_type_id;
    const std::atomic<uint64_t>* seq_addr{nullptr};
    uint64_t observed_seq{0};
    uint32_t slot_count{0};

    auto stable() const -> bool {
        if (seq_addr == nullptr) {
            return true;
        }
        const auto now = seq_addr->load(std::memory_order_acquire);
        // 槽位按 seq % slot_count 分配：本槽只有在 seq 推进满 slot_count 后才会被覆写。
        // 跨槽写入不应误判本视图失效（否则同通道的并发写会让无关视图集体失效）。
        if (slot_count == 0) {
            return now == observed_seq;
        }
        return now >= observed_seq && (now - observed_seq) < slot_count;
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

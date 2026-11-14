#ifndef __SHM_FRAME_H__
#define __SHM_FRAME_H__

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "basement/core/type_id.h"

namespace star::transport {

// shm 槽内帧格式（v7 起）：
//
//   magic(2) | encoding(1) | flags(1) | name_len(2) | reserved(2)
//   | schema_version(8) | schema_hash(8) | payload_size(4)
//   | schema_name(name_len) | payload(payload_size)
//
// 为什么逐帧带身份：滚动升级时同一个通道上会同时出现 v1/v2 的生产者；
// 「通道级身份」会在首写时被冻结，无法表达后续版本。身份必须在帧上。
// schema name 逐帧携带（通常 < 32 字节），换来的可诊断性远大于开销。
#pragma pack(push, 1)
struct ShmFrameHeader {
    uint16_t magic{0};
    uint8_t encoding{0};
    uint8_t flags{0};
    uint16_t name_len{0};
    uint16_t reserved{0};
    int64_t schema_version{0};
    uint64_t schema_hash{0};
    uint32_t payload_size{0};
};
#pragma pack(pop)

static_assert(sizeof(ShmFrameHeader) == 28, "ShmFrameHeader layout changed unexpectedly");

inline constexpr uint16_t kShmFrameMagic = 0x5346;  // 'S','F'
inline constexpr uint16_t kShmFrameMaxNameLen = 255;

enum class ShmEncoding : uint8_t {
    kTlv = 1,
    kFixedLayout = 2,
};

struct ShmFrameView {
    ShmEncoding encoding{ShmEncoding::kTlv};
    star::schema::SchemaTypeId schema_type_id{};
    const uint8_t* payload{nullptr};
    uint32_t payload_size{0};
};
#pragma pack(pop)

inline auto parse_shm_frame(std::span<const uint8_t> bytes) -> std::optional<ShmFrameView> {
    if (bytes.size() < sizeof(ShmFrameHeader)) {
        return std::nullopt;
    }
    auto header = ShmFrameHeader{};
    std::memcpy(&header, bytes.data(), sizeof(header));
    if (header.magic != kShmFrameMagic or header.name_len > kShmFrameMaxNameLen) {
        return std::nullopt;
    }
    if (header.encoding != static_cast<uint8_t>(ShmEncoding::kTlv) and
        header.encoding != static_cast<uint8_t>(ShmEncoding::kFixedLayout)) {
        return std::nullopt;
    }
    const auto header_bytes = sizeof(ShmFrameHeader) + static_cast<size_t>(header.name_len);
    if (header_bytes > bytes.size() or header.payload_size > bytes.size() - header_bytes) {
        return std::nullopt;
    }
    auto out = ShmFrameView{};
    out.encoding = static_cast<ShmEncoding>(header.encoding);
    out.schema_type_id.name.assign(reinterpret_cast<const char*>(bytes.data() + sizeof(ShmFrameHeader)), header.name_len);
    out.schema_type_id.version = header.schema_version;
    out.schema_type_id.hash = header.schema_hash;
    out.payload = bytes.data() + header_bytes;
    out.payload_size = header.payload_size;
    return out;
}

}  // namespace star::transport

#endif  // __SHM_FRAME_H__

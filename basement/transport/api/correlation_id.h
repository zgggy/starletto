#ifndef __CORRELATION_ID_H__
#define __CORRELATION_ID_H__

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace star::transport {

struct CorrelationId {
    uint64_t value{0};

    auto is_valid() const -> bool { return value != 0; }

    auto operator==(const CorrelationId& other) const -> bool { return value == other.value; }

    static auto generate() -> CorrelationId {
        static std::atomic<uint64_t> next_id{1};
        return CorrelationId{next_id.fetch_add(1, std::memory_order_relaxed)};
    }
};

}  // namespace star::transport

namespace std {

template <>
struct hash<star::transport::CorrelationId> {
    auto operator()(const star::transport::CorrelationId& correlation_id) const -> size_t { return std::hash<uint64_t>{}(correlation_id.value); }
};

}  // namespace std

#endif  // __CORRELATION_ID_H__

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
        // 唯一性不依赖「函数内 static 在各 DSO 间被符号统一」这一实现细节：
        // 插件以 RTLD_LOCAL 加载时每个 DSO 各有一份 next_id；若只用单调计数，
        // 不同 DSO 的 id 会碰撞，导致 service 响应被匹配给别的请求（静默串台）。
        // 高位放本 DSO 独有的命名空间、低位留给计数器，两种情况下都保证不碰撞。
        static const uint64_t kNamespace = derive_namespace();
        const auto seq = next_id.fetch_add(1, std::memory_order_relaxed);
        return CorrelationId{(kNamespace << 32) | (seq & 0xffffffffULL)};
    }

  private:
    static auto derive_namespace() -> uint64_t {
        // 取本 DSO 内静态对象地址派生：符号被统一时各 DSO 同值（退化为纯计数），
        // 未被统一时各 DSO 异值（命名空间隔离）
        static const int anchor = 0;
        return std::hash<const void*>{}(static_cast<const void*>(&anchor)) & 0xffffffffULL;
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

#ifndef __STAR_CORE_STABLE_HASH_H__
#define __STAR_CORE_STABLE_HASH_H__

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace star::base {

// 跨进程/跨工具链稳定的 64 位哈希（FNV-1a）。
//
// 不能用于：进程内的 unordered_map（std::hash 更快）。
// 必须用于：写进共享内存段名、文件名、wire 标识等「另一个进程/另一个编译器
// 要重新算一遍」的地方。std::hash<std::string> 是实现定义的（libc++ 与
// libstdc++ 算法/种子不同），跨工具链会算出不同段名，双方各自 attach 到
// 不同段且都显示成功——典型的静默分裂。
inline auto fnv1a64(std::string_view text) -> uint64_t {
    uint64_t value = 0xCBF29CE484222325ULL;
    for (const unsigned char byte : text) {
        value ^= static_cast<uint64_t>(byte);
        value *= 0x100000001B3ULL;
    }
    return value;
}

inline auto fnv1a64(const char* text) -> uint64_t { return fnv1a64(std::string_view{text == nullptr ? "" : text}); }

}  // namespace star::base

#endif  // __STAR_CORE_STABLE_HASH_H__

#ifndef __STAR_CORE_FUTEX_COMPAT_H__
#define __STAR_CORE_FUTEX_COMPAT_H__

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

#if defined(__linux__)
#include <linux/futex.h>
#include <sys/syscall.h>
#endif

namespace star::base {

// 跨进程共享内存上的等待/唤醒原语。
// Linux 走 futex；其余平台无对等原语，退化为短轮询——语义等价
// （调用方本就以谓词循环 + 超时返回），仅效率有别。
// timeout_ms < 0 表示无限等待；返回值 0 表示被唤醒/条件已变，-1 表示超时。
inline auto futex_wait(std::atomic<uint32_t>* addr, uint32_t expected, int64_t timeout_ms) -> int {
#if defined(__linux__)
    auto ts = ::timespec{};
    auto* tsp = static_cast<::timespec*>(nullptr);
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = static_cast<long>((timeout_ms % 1000) * 1000000);
        tsp = &ts;
    }
    return static_cast<int>(::syscall(SYS_futex, reinterpret_cast<uint32_t*>(addr), FUTEX_WAIT, expected, tsp, nullptr, 0));
#else
    const auto infinite = timeout_ms < 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(infinite ? 0 : timeout_ms);
    while (addr->load(std::memory_order_acquire) == expected) {
        if (not infinite and std::chrono::steady_clock::now() >= deadline) {
            return -1;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    return 0;
#endif
}

inline void futex_wake(std::atomic<uint32_t>* addr, int count) {
#if defined(__linux__)
    ::syscall(SYS_futex, reinterpret_cast<uint32_t*>(addr), FUTEX_WAKE, count, nullptr, nullptr, 0);
#else
    // 轮询退化下无需显式唤醒：调用方总是先改动 *addr，等待方下一轮询即可见
    (void)addr;
    (void)count;
#endif
}

}  // namespace star::base

#endif  // __STAR_CORE_FUTEX_COMPAT_H__

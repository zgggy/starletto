#ifndef __STAR_RUNTIME_CONTEXT_FRAME_CONTEXT_H__
#define __STAR_RUNTIME_CONTEXT_FRAME_CONTEXT_H__

#include <chrono>
#include <cstdint>

namespace star::runtime {

struct FrameContext {
    int64_t frame_id{0};
    int64_t monotonic_time_ns{0};

    static auto now_monotonic_ns() -> int64_t {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    static auto default_context() -> FrameContext {
        return {0, 0};
    }
};

}  // namespace star::runtime

#endif  // __STAR_RUNTIME_CONTEXT_FRAME_CONTEXT_H__

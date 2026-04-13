#ifndef __FRAME_CONTEXT_H__
#define __FRAME_CONTEXT_H__

#include <cstddef>
#include <cstdint>

namespace star::planning::data {

struct FrameContext {
    size_t frame_seq = 0;
    uint64_t cycle_count = 0;
    double last_latency_ms = 0.0;
};

} // namespace star::planning::data

#endif // __FRAME_CONTEXT_H__

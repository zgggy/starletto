#ifndef __TRANSPORT_PERCENTILE_H__
#define __TRANSPORT_PERCENTILE_H__

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace star::transport {

inline auto compute_percentile(const std::vector<uint64_t>& samples, double p) -> uint64_t {
    if (samples.empty()) {
        return 0;
    }
    if (p <= 0.0) {
        return *std::min_element(samples.begin(), samples.end());
    }
    if (p >= 1.0) {
        return *std::max_element(samples.begin(), samples.end());
    }
    auto tmp = samples;
    const auto rank = static_cast<size_t>(std::ceil(p * static_cast<double>(tmp.size())));
    const auto idx = std::min(rank > 0 ? (rank - 1) : 0, tmp.size() - 1);
    std::nth_element(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(idx), tmp.end());
    return tmp[idx];
}

}  // namespace star::transport

#endif  // __TRANSPORT_PERCENTILE_H__

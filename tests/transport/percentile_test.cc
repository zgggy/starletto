#include "basement/transport/common/percentile.h"

#include "tests/support/star_test.h"

namespace {

using star::transport::compute_percentile;

TEST_CASE("compute_percentile empty samples returns zero") {
    REQUIRE_EQ(compute_percentile({}, 0.5), 0ULL);
}

TEST_CASE("compute_percentile clamps p to min and max") {
    const auto samples = std::vector<uint64_t>{10, 20, 30, 40};
    REQUIRE_EQ(compute_percentile(samples, -1.0), 10ULL);
    REQUIRE_EQ(compute_percentile(samples, 0.0), 10ULL);
    REQUIRE_EQ(compute_percentile(samples, 1.0), 40ULL);
    REQUIRE_EQ(compute_percentile(samples, 2.0), 40ULL);
}

TEST_CASE("compute_percentile uses nearest-rank indexing") {
    const auto samples = std::vector<uint64_t>{10, 20, 30, 40};
    // rank = ceil(p * n)；p=0.5 -> rank 2 -> 下标 1
    REQUIRE_EQ(compute_percentile(samples, 0.5), 20ULL);
    REQUIRE_EQ(compute_percentile(samples, 0.25), 10ULL);
    REQUIRE_EQ(compute_percentile(samples, 0.75), 30ULL);
    REQUIRE_EQ(compute_percentile(samples, 0.99), 40ULL);
}

TEST_CASE("compute_percentile does not mutate the input samples") {
    auto samples = std::vector<uint64_t>{40, 10, 30, 20};
    (void)compute_percentile(samples, 0.5);
    REQUIRE_EQ(samples.size(), 4U);
    REQUIRE_EQ(samples[0], 40ULL);
    REQUIRE_EQ(samples[1], 10ULL);
}

TEST_CASE("compute_percentile handles single sample") {
    const auto samples = std::vector<uint64_t>{7};
    REQUIRE_EQ(compute_percentile(samples, 0.5), 7ULL);
    REQUIRE_EQ(compute_percentile(samples, 0.99), 7ULL);
}

}  // namespace

#include <dlfcn.h>

#include <cstdint>
#include <set>
#include <string>

#include "basement/transport/api/correlation_id.h"

#include "tests/support/star_test.h"

namespace {

using star::transport::CorrelationId;

TEST_CASE("CorrelationId generate returns valid unique ids") {
    auto seen = std::set<uint64_t>{};
    for (int i = 0; i < 10000; ++i) {
        const auto id = CorrelationId::generate();
        REQUIRE(id.is_valid());
        REQUIRE(seen.insert(id.value).second);
    }
}

TEST_CASE("CorrelationId keeps the counter in the low bits") {
    const auto first = CorrelationId::generate().value;
    const auto second = CorrelationId::generate().value;
    // 同一 DSO 内命名空间不变，低位计数器递增
    REQUIRE_EQ(first >> 32, second >> 32);
    REQUIRE((second & 0xffffffffULL) > (first & 0xffffffffULL));
}

// P1-6 回归测试：插件以 RTLD_LOCAL 加载时每个 DSO 各有一份 next_id。
// 若 id 只用单调计数，主程序与插件会各自从 1 开始而碰撞，导致 service
// 响应被匹配给别的请求（静默串台）。此处在两个 DSO 内交替生成 id 并断言不碰撞。
TEST_CASE("CorrelationId does not collide across separately loaded DSOs") {
#ifndef STAR_TEST_PLUGIN_PATH
    throw star::test::Failure{"STAR_TEST_PLUGIN_PATH 未定义，无法执行多 DSO 测试"};
#else
    void* handle = ::dlopen(STAR_TEST_PLUGIN_PATH, RTLD_NOW | RTLD_LOCAL);
    REQUIRE(handle != nullptr);
    using GenerateFn = uint64_t (*)();
    const auto plugin_generate = reinterpret_cast<GenerateFn>(::dlsym(handle, "test_generate_correlation_id"));
    REQUIRE(plugin_generate != nullptr);

    auto seen = std::set<uint64_t>{};
    for (int i = 0; i < 2000; ++i) {
        const auto from_host = CorrelationId::generate().value;
        const auto from_plugin = plugin_generate();
        REQUIRE(seen.insert(from_host).second);
        REQUIRE(seen.insert(from_plugin).second);
        REQUIRE(from_host != from_plugin);
    }

    ::dlclose(handle);
#endif
}

}  // namespace

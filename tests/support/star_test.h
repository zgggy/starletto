#ifndef __STAR_TEST_H__
#define __STAR_TEST_H__

// 轻量单元测试框架（Catch2 风格 API 子集）。
// 本机离线无法拉取 Catch2/googletest，故内置最小实现；接口刻意对齐
// TEST_CASE / REQUIRE / CHECK / REQUIRE_EQ，后续可平移到正式框架。

#include <cstdio>
#include <exception>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace star::test {

struct TestCase {
    std::string name;
    std::function<void()> fn;
};

inline auto registry() -> std::vector<TestCase>& {
    static auto cases = std::vector<TestCase>{};
    return cases;
}

inline auto failure_count() -> int& {
    static int count = 0;
    return count;
}

struct Failure {
    std::string message;
};

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back(TestCase{name, std::move(fn)}); }
};

inline void report_failure(const char* expr, const char* file, int line, const std::string& detail = {}) {
    ++failure_count();
    std::printf("    FAIL %s:%d  %s%s\n", file, line, expr, detail.c_str());
}

template <typename _T, typename = void>
struct is_streamable : std::false_type {};

template <typename _T>
struct is_streamable<_T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const _T&>())>> : std::true_type {};

// 对不可流化的类型（如 std::vector<uint8_t>）降级输出，避免 REQUIRE_EQ 编译失败
template <typename _A, typename _B>
auto describe_eq(const _A& lhs, const _B& rhs) -> std::string {
    auto os = std::ostringstream{};
    os << "  [lhs=";
    if constexpr (is_streamable<_A>::value) {
        os << lhs;
    } else {
        os << "<unprintable>";
    }
    os << " rhs=";
    if constexpr (is_streamable<_B>::value) {
        os << rhs;
    } else {
        os << "<unprintable>";
    }
    os << "]";
    return os.str();
}

inline auto run_all() -> int {
    auto passed = 0;
    auto failed = 0;
    for (const auto& test_case : registry()) {
        const auto before = failure_count();
        std::printf("[ RUN ] %s\n", test_case.name.c_str());
        try {
            test_case.fn();
        } catch (const Failure& failure) {
            std::printf("    FAIL %s\n", failure.message.c_str());
            ++failure_count();
        } catch (const std::exception& ex) {
            std::printf("    FAIL unexpected exception: %s\n", ex.what());
            ++failure_count();
        } catch (...) {
            std::printf("    FAIL unknown exception\n");
            ++failure_count();
        }
        if (failure_count() == before) {
            std::printf("[ OK ] %s\n", test_case.name.c_str());
            ++passed;
        } else {
            std::printf("[ FAILED ] %s\n", test_case.name.c_str());
            ++failed;
        }
    }
    std::printf("== star_test: %d passed, %d failed, %zu total ==\n", passed, failed, registry().size());
    return failed == 0 ? 0 : 1;
}

}  // namespace star::test

#define STAR_TEST_CAT_IMPL(a, b) a##b
#define STAR_TEST_CAT(a, b) STAR_TEST_CAT_IMPL(a, b)
#define STAR_TEST_CASE_DECL(name, line)                                                  \
    static void STAR_TEST_CAT(star_test_fn_, line)();                                    \
    static ::star::test::Registrar STAR_TEST_CAT(star_test_reg_, line){name, &STAR_TEST_CAT(star_test_fn_, line)}; \
    static void STAR_TEST_CAT(star_test_fn_, line)()

#define TEST_CASE(name) STAR_TEST_CASE_DECL(name, __LINE__)

#define REQUIRE(expr)                                                                 \
    do {                                                                              \
        if (!(expr)) {                                                                \
            throw ::star::test::Failure{std::string("REQUIRE(" #expr ") failed at ") + \
                                        __FILE__ + ":" + std::to_string(__LINE__)};   \
        }                                                                             \
    } while (0)

#define REQUIRE_EQ(lhs, rhs)                                                                                     \
    do {                                                                                                         \
        const auto& star_test_lhs = (lhs);                                                                       \
        const auto& star_test_rhs = (rhs);                                                                       \
        if (!(star_test_lhs == star_test_rhs)) {                                                                 \
            throw ::star::test::Failure{std::string("REQUIRE_EQ(" #lhs ", " #rhs ") failed at ") + __FILE__ +    \
                                        ":" + std::to_string(__LINE__) +                                         \
                                        ::star::test::describe_eq(star_test_lhs, star_test_rhs)};                \
        }                                                                                                        \
    } while (0)

#define CHECK(expr)                                                             \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ::star::test::report_failure("CHECK(" #expr ")", __FILE__, __LINE__); \
        }                                                                       \
    } while (0)

#endif  // __STAR_TEST_H__

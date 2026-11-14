#ifndef __STAR_CORE_BUILD_INFO_H__
#define __STAR_CORE_BUILD_INFO_H__

#include <cstddef>
#include <string>

namespace star::base {

// 插件 ABI 契约：当前运行时是 header-only，插件与宿主必须用同一工具链/标准库/
// 关键 ABI 选项编译。这个指纹把「同工具链」从文档假设变成可检查的字符串。
//
// 注意：它不是加密签名，只用于发现混编；不能替代共享库/稳定 ABI。
inline auto toolchain_fingerprint() -> const char* {
    static const std::string fingerprint = []() {
        auto out = std::string{};
#if defined(__clang__)
        out += "clang-" __VERSION__;
#elif defined(__GNUC__)
        out += "gcc-" __VERSION__;
#else
        out += "unknown-compiler";
#endif
#if defined(_LIBCPP_VERSION)
        out += ";libc++-" + std::to_string(_LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
        out += ";libstdc++-" + std::to_string(__GLIBCXX__);
#else
        out += ";unknown-stdlib";
#endif
#if defined(_GLIBCXX_USE_CXX11_ABI)
        out += ";cxx11abi=" + std::to_string(_GLIBCXX_USE_CXX11_ABI);
#endif
#if defined(_LIBCPP_ABI_VERSION)
        out += ";libcpp-abi=" + std::to_string(_LIBCPP_ABI_VERSION);
#endif
        out += ";cplusplus=" + std::to_string(__cplusplus);
        out += ";ptr=" + std::to_string(sizeof(void*));
#if defined(NDEBUG)
        out += ";ndebug=1";
#else
        out += ";ndebug=0";
#endif
        return out;
    }();
    return fingerprint.c_str();
}

}  // namespace star::base

// 插件入口附带导出该符号；宿主 doctor --deep / 加载器据此比对工具链。
#define STAR_EXPORT_TOOLCHAIN_FINGERPRINT() \
    extern "C" const char* star_toolchain_fingerprint() { return star::base::toolchain_fingerprint(); }

#endif  // __STAR_CORE_BUILD_INFO_H__

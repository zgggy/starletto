#ifndef __STAR_TRANSPORT_CHANNEL_ADDRESS_H__
#define __STAR_TRANSPORT_CHANNEL_ADDRESS_H__

#include <cctype>
#include <string_view>

namespace star::transport {

// 通道是数据存放地址：绝对路径、生产者在前，例如 /perception/prediction。
// schema 类型名才用点（perception.prediction），两者不要混用。
inline auto is_valid_channel_address(std::string_view address) -> bool {
    if (address.size() < 2 || address.front() != '/' || address.back() == '/') {
        return false;
    }
    size_t start = 1;
    while (start < address.size()) {
        const auto slash = address.find('/', start);
        const auto end = slash == std::string_view::npos ? address.size() : slash;
        if (end == start) {
            return false;  // 空段 / 双斜线
        }
        for (size_t i = start; i < end; ++i) {
            const auto ch = static_cast<unsigned char>(address[i]);
            if (not(std::isalnum(ch) != 0 || ch == '_' || ch == '-')) {
                return false;  // 不允许点、空格等；段只用 [A-Za-z0-9_-]
            }
        }
        if (slash == std::string_view::npos) {
            break;
        }
        start = slash + 1;
    }
    return true;
}

}  // namespace star::transport

#endif  // __STAR_TRANSPORT_CHANNEL_ADDRESS_H__

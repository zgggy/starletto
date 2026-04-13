#ifndef __TO_STRING_WITH_PRECISION_H__
#define __TO_STRING_WITH_PRECISION_H__

#include <sstream>
#include <string>

template <typename T>
inline std::string to_string_with_precision(const T a_value, const int n = 6) {
    std::ostringstream out;
    out.precision(n);
    out << std::fixed << a_value;
    return out.str();
}

#endif  // __TO_STRING_WITH_PRECISION_H__
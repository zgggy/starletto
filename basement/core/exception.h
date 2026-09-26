#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace star::excep {

template <typename... Args>
std::string str_cat(const Args&... args) {
    std::ostringstream oss;
    (oss << ... << args);
    return oss.str();
}

class StarException : public std::exception {
  public:
    StarException(std::string_view message) : message_(static_cast<std::string>(message)) {}

    template <typename... SV>
    StarException(const SV&... args) : message_(str_cat(args...)) {}

    const char* what() const noexcept override { return message_.c_str(); }

  private:
    std::string message_;
};

// Broken internal invariant (e.g. an assertion the code should never reach).
// Fixing these requires a code change, not a retry.
class BugError : public StarException {
  public:
    BugError(std::string_view message) : StarException(message) {}

    template <typename... SV>
    BugError(const SV&... args) : StarException(args...) {}
};

// The call itself is invalid: a bad argument combination, a use-before-init, or
// a mis-ordered lifecycle step. Fixing these requires adjusting the caller's logic.
class LogicError : public StarException {
  public:
    LogicError(std::string_view message) : StarException(message) {}

    template <typename... SV>
    LogicError(const SV&... args) : StarException(args...) {}
};

// Data or environment conditions seen only at run-time (missing file, unavailable
// resource, malformed input). These may recover on a later attempt.
class RuntimeError : public StarException {
  public:
    RuntimeError(std::string_view message) : StarException(message) {}

    template <typename... SV>
    RuntimeError(const SV&... args) : StarException(args...) {}
};

}  // namespace star::excep

#endif  // __EXCEPTION_H__

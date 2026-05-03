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

    const char* what() const noexcept { return message_.c_str(); }

  private:
    std::string message_;
};

// This errors are usually related to problems which "probably" require code refactoring
// to be fixed.
class BugError : public StarException {
  public:
    BugError(std::string_view message) : StarException(message) {}

    template <typename... SV>
    BugError(const SV&... args) : StarException(args...) {}
};

// This errors are usually related to problems which "probably" require code refactoring
// to be fixed.
class LogicError : public StarException {
  public:
    LogicError(std::string_view message) : StarException(message) {}

    template <typename... SV>
    LogicError(const SV&... args) : StarException(args...) {}
};

// This errors are usually related to problems that are related to data or conditions
// that happen only at run-time
class RuntimeError : public StarException {
  public:
    RuntimeError(std::string_view message) : StarException(message) {}

    template <typename... SV>
    RuntimeError(const SV&... args) : StarException(args...) {}
};

}  // namespace star::excep

namespace star::base {
using StarException = star::excep::StarException;
using BugError = star::excep::BugError;
using LogicError = star::excep::LogicError;
using RuntimeError = star::excep::RuntimeError;
}  // namespace star::base

#endif  // __EXCEPTION_H__

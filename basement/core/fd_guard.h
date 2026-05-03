#ifndef __CORE_FD_GUARD_H__
#define __CORE_FD_GUARD_H__

#include <cerrno>
#include <cstdio>
#include <unistd.h>

#include <utility>

namespace star::base {

class FdGuard {
  public:
    FdGuard() = default;
    explicit FdGuard(int fd) : fd_(fd) {}

    FdGuard(const FdGuard&) = delete;
    auto operator=(const FdGuard&) -> FdGuard& = delete;

    FdGuard(FdGuard&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    auto operator=(FdGuard&& other) noexcept -> FdGuard& {
        if (this == &other) {
            return *this;
        }
        reset();
        fd_ = std::exchange(other.fd_, -1);
        return *this;
    }

    ~FdGuard() { reset(); }

    auto get() const -> int { return fd_; }

    auto release() -> int { return std::exchange(fd_, -1); }

    void reset() {
        if (fd_ >= 0) {
            last_close_errno_ = 0;
            if (::close(fd_) != 0) {
                last_close_errno_ = errno;
            }
            fd_ = -1;
        }
    }

    explicit operator bool() const { return fd_ >= 0; }

    auto last_close_errno() const -> int { return last_close_errno_; }

  private:
    int fd_{-1};
    int last_close_errno_{0};
};

class FileGuard {
  public:
    FileGuard() = default;
    explicit FileGuard(FILE* fp) : fp_(fp) {}

    FileGuard(const FileGuard&) = delete;
    auto operator=(const FileGuard&) -> FileGuard& = delete;

    FileGuard(FileGuard&& other) noexcept : fp_(std::exchange(other.fp_, nullptr)) {}

    auto operator=(FileGuard&& other) noexcept -> FileGuard& {
        if (this == &other) {
            return *this;
        }
        reset();
        fp_ = std::exchange(other.fp_, nullptr);
        return *this;
    }

    ~FileGuard() { reset(); }

    auto get() const -> FILE* { return fp_; }

    auto release() -> FILE* { return std::exchange(fp_, nullptr); }

    void reset() {
        if (fp_ != nullptr) {
            last_close_errno_ = 0;
            if (std::fclose(fp_) != 0) {
                last_close_errno_ = errno;
            }
            fp_ = nullptr;
        }
    }

    explicit operator bool() const { return fp_ != nullptr; }

    auto last_close_errno() const -> int { return last_close_errno_; }

  private:
    FILE* fp_{nullptr};
    int last_close_errno_{0};
};

}  // namespace star::base

#endif  // __CORE_FD_GUARD_H__

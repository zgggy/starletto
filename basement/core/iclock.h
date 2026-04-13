#ifndef __BASEMENT_ICLOCK_H__
#define __BASEMENT_ICLOCK_H__

#include <chrono>
#include <cstdint>

namespace star::base {

class IClock {
  public:
    virtual ~IClock() = default;

    virtual auto now() const -> std::chrono::nanoseconds = 0;
    virtual auto steady_now() const -> std::chrono::nanoseconds = 0;
};

class SteadyClock final : public IClock {
  public:
    auto now() const -> std::chrono::nanoseconds override { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()); }

    auto steady_now() const -> std::chrono::nanoseconds override { return now(); }
};

class SystemClock final : public IClock {
  public:
    auto now() const -> std::chrono::nanoseconds override { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()); }

    auto steady_now() const -> std::chrono::nanoseconds override { return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()); }
};

class SimulationClock final : public IClock {
  public:
    explicit SimulationClock(std::chrono::nanoseconds start = {}) : current_(start) {}

    auto now() const -> std::chrono::nanoseconds override { return current_; }
    auto steady_now() const -> std::chrono::nanoseconds override { return current_; }

    void advance(std::chrono::nanoseconds delta) { current_ += delta; }
    void set(std::chrono::nanoseconds time) { current_ = time; }
    auto current() const -> std::chrono::nanoseconds { return current_; }

  private:
    std::chrono::nanoseconds current_;
};

}  // namespace star::base

#endif  // __BASEMENT_ICLOCK_H__

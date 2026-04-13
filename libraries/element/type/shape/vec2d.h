#ifndef __VEC2D_H__
#define __VEC2D_H__

#include <cmath>
#include <string>

#include "libraries/element/math_constants.h"

namespace star::base {

class Vec2D {
  protected:
    double x_;
    double y_;

  public:
    Vec2D() : x_(0), y_(0) {}
    Vec2D(const double x, const double y) : x_(x), y_(y) {}
    Vec2D(const Vec2D& vec) : x_(vec.x()), y_(vec.y()) {}
    Vec2D(const double angle) : x_(cos(angle)), y_(sin(angle)) {}

    auto x() const -> double { return x_; }
    auto y() const -> double { return y_; }
    auto set_x(double x) -> void { x_ = x; }
    auto set_y(double y) -> void { y_ = y; }

    auto operator+(const Vec2D& other) const -> Vec2D { return Vec2D(x() + other.x(), y() + other.y()); }
    auto operator-(const Vec2D& other) const -> Vec2D { return Vec2D(x() - other.x(), y() - other.y()); }
    auto operator*(const Vec2D& other) const -> double { return x() * other.x() + y() * other.y(); }
    auto operator*(const double other) const -> Vec2D { return Vec2D(x() * other, y() * other); }

    template <typename _VecType>
    auto operator==(const _VecType& other) const -> bool {
        return (fabs(this->x() - other.x()) < kEpsilon) and (fabs(this->y() - other.y()) < kEpsilon);
    }
    template <typename _VecType>
    auto operator!=(const _VecType& other) const -> bool {
        return (fabs(this->x() - other.x()) > kEpsilon) or (fabs(this->y() - other.y()) > kEpsilon);
    }
    auto outer(const Vec2D& other) const -> double { return x() * other.y() - y() * other.x(); }
    auto cross(const Vec2D& other) const -> double { return outer(other); }
    auto inner(const Vec2D& other) const -> double { return x() * other.x() + y() * other.y(); }
    auto dot(const Vec2D& other) const -> double { return inner(other); }

    auto mod() const -> double { return std::hypot(x(), y()); }
    auto normalize() const -> Vec2D {
        const double l = mod();
        return l > kEpsilon ? Vec2D(x() / l, y() / l) : Vec2D(0, 0);
    }
    auto angle() const -> double { return std::atan2(y(), x()); }
    auto angle_diff_with(const Vec2D& other) const -> double {
        double diff = std::fmod(other.angle() - angle() + kPi, 2.0 * kPi);
        if (diff < 0.0) diff += 2.0 * kPi;
        return diff - kPi;
    }
    auto rotate(const double angle) const -> Vec2D { return Vec2D(x() * cos(angle) - y() * sin(angle), x() * sin(angle) + y() * cos(angle)); }

    template <typename _VecType>
    auto distance_with(const _VecType& other) const -> double {
        return (*this - other).mod();
    }

    template <typename _VecType>
    auto distance_square_with(const _VecType& other) const -> double {
        auto dis_vec = *this - other;
        return dis_vec.x() * dis_vec.x() + dis_vec.y() * dis_vec.y();
    }

    template <typename _VecType>
    auto near(const _VecType& other, double distance) const -> bool {
        return (fabs(this->x() - other.x()) < distance) and (fabs(this->y() - other.y()) < distance);
    }
    virtual auto str() const -> std::string { return "V(" + std::to_string(x()) + "," + std::to_string(y()) + ")"; }
};

}  // namespace star::base

#endif  // __VEC2D_H__

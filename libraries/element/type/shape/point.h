#ifndef __POINT_H__
#define __POINT_H__

#include "vec2d.h"

namespace star::base {

class Pose : public Vec2D {
  protected:
    double theta_;
    double s_;

  public:
    Pose() : Vec2D(), theta_(0), s_(0) {}
    Pose(const double x, const double y) : Vec2D(x, y), theta_(0), s_(0) {}
    Pose(const Vec2D& vec) : Vec2D(vec), theta_(0), s_(0) {}
    Pose(const double x, const double y, const double theta) : Vec2D(x, y), theta_(theta), s_(0) {}
    Pose(const Vec2D& vec, const double theta) : Vec2D(vec), theta_(theta), s_(0) {}
    Pose(const Pose& point) : Vec2D(point), theta_(point.theta()), s_(point.s()) {}

    auto theta() const -> double { return theta_; }
    auto s() const -> double { return s_; }
    auto set_theta(double theta) -> void { theta_ = theta; }
    auto set_s(double s) -> void { s_ = s; }

    auto vec() const -> Vec2D { return Vec2D(x(), y()); }

    virtual auto str() const -> std::string { return "P(" + std::to_string(x()) + "," + std::to_string(y()) + "," + std::to_string(theta()) + ")"; }
};

class FrenetPoint {
  protected:
    double s_;
    double l_;
    double dl_;
    double ddl_;
    double dddl_;

  public:
    FrenetPoint() : s_(0), l_(0), dl_(0), ddl_(0), dddl_(0) {}

    auto s() const -> double { return s_; }
    auto l() const -> double { return l_; }
    auto dl() const -> double { return dl_; }
    auto ddl() const -> double { return ddl_; }
    auto dddl() const -> double { return dddl_; }
    auto set_s(double s) -> void { s_ = s; }
    auto set_l(double l) -> void { l_ = l; }
    auto set_dl(double dl) -> void { dl_ = dl; }
    auto set_ddl(double ddl) -> void { ddl_ = ddl; }
    auto set_dddl(double dddl) -> void { dddl_ = dddl; }
};

class SpeedPoint {
  protected:
    double t_;
    double s_;
    double v_;
    double a_;
    double da_;

  public:
    SpeedPoint() : t_(0), s_(0), v_(0), a_(0), da_(0) {}

    auto t() const -> double { return t_; }
    auto s() const -> double { return s_; }
    auto v() const -> double { return v_; }
    auto a() const -> double { return a_; }
    auto da() const -> double { return da_; }
    auto set_t(double t) -> void { t_ = t; }
    auto set_s(double s) -> void { s_ = s; }
    auto set_v(double v) -> void { v_ = v; }
    auto set_a(double a) -> void { a_ = a; }
    auto set_da(double da) -> void { da_ = da; }
};

class PathPoint : public Pose {
  protected:
    double kappa_;
    double dkappa_;
    double ddkappa_;

  public:
    PathPoint() : Pose(), kappa_(0), dkappa_(0), ddkappa_(0) {}

    auto kappa() const -> double { return kappa_; }
    auto dkappa() const -> double { return dkappa_; }
    auto ddkappa() const -> double { return ddkappa_; }
    auto set_kappa(double kappa) -> void { kappa_ = kappa; }
    auto set_dkappa(double dkappa) -> void { dkappa_ = dkappa; }
    auto set_ddkappa(double ddkappa) -> void { ddkappa_ = ddkappa; }
};

class ReferencePoint : public PathPoint {
  protected:
    double left_;
    double right_;

  public:
    ReferencePoint() : PathPoint(), left_(0), right_(0) {}

    auto left() const -> double { return left_; }
    auto right() const -> double { return right_; }
    auto set_left(double left) -> void { left_ = left; }
    auto set_right(double right) -> void { right_ = right; }
};

class TrajPoint : public PathPoint {
  protected:
    double t_;
    double v_;
    double a_;
    double da_;
    double d_;

  public:
    TrajPoint() : PathPoint(), t_(0), v_(0), a_(0), da_(0), d_(0) {}

    auto t() const -> double { return t_; }
    auto v() const -> double { return v_; }
    auto a() const -> double { return a_; }
    auto da() const -> double { return da_; }
    auto d() const -> double { return d_; }
    auto set_t(double t) -> void { t_ = t; }
    auto set_v(double v) -> void { v_ = v; }
    auto set_a(double a) -> void { a_ = a; }
    auto set_da(double da) -> void { da_ = da; }
    auto set_d(double d) -> void { d_ = d; }
};

}  // namespace star::base

#endif  // __POINT_H__

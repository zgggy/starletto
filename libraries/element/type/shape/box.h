#ifndef __BOX_H__
#define __BOX_H__

#include <cmath>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>
#include <limits>

#include "point.h"

namespace star::base {

class Box {
  protected:
    Pose center_;
    double length_to_front_;
    double length_to_back_;
    double width_to_left_;
    double width_to_right_;

  public:
    Box() : center_(0, 0), length_to_front_(0), length_to_back_(0), width_to_left_(0), width_to_right_(0) {}
    Box(const Pose& center, const double length, const double width) : center_(center), length_to_front_(length), length_to_back_(length), width_to_left_(width), width_to_right_(width) {}
    Box(const Pose& center, const double length_to_front, const double length_to_back, const double width)
        : center_(center), length_to_front_(length_to_front), length_to_back_(length_to_back), width_to_left_(width), width_to_right_(width) {}
    Box(const Pose& center, const double length_to_front, const double length_to_back, const double width_to_left, const double width_to_right)
        : center_(center), length_to_front_(length_to_front), length_to_back_(length_to_back), width_to_left_(width_to_left), width_to_right_(width_to_right) {}

    auto center() const -> Pose { return center_; }
    auto length_to_front() const -> double { return length_to_front_; }
    auto length_to_back() const -> double { return length_to_back_; }
    auto width_to_left() const -> double { return width_to_left_; }
    auto width_to_right() const -> double { return width_to_right_; }
    auto set_center(const Pose& center) -> void { center_ = center; }
    auto set_length_to_front(double length_to_front) -> void { length_to_front_ = length_to_front; }
    auto set_length_to_back(double length_to_back) -> void { length_to_back_ = length_to_back; }
    auto set_width_to_left(double width_to_left) -> void { width_to_left_ = width_to_left; }
    auto set_width_to_right(double width_to_right) -> void { width_to_right_ = width_to_right; }

    auto length() const -> double { return length_to_front() + length_to_back(); }
    auto width() const -> double { return width_to_left() + width_to_right(); }
    auto fl() const -> Vec2D { return center() + Vec2D(length_to_front(), width_to_left()).rotate(center().theta()); }
    auto fr() const -> Vec2D { return center() + Vec2D(length_to_front(), -width_to_right()).rotate(center().theta()); }
    auto bl() const -> Vec2D { return center() + Vec2D(-length_to_back(), width_to_left()).rotate(center().theta()); }
    auto br() const -> Vec2D { return center() + Vec2D(-length_to_back(), -width_to_right()).rotate(center().theta()); }

    auto corner_points() const -> std::vector<Pose> { return std::vector<Pose>{fl(), fr(), br(), bl()}; }
    auto aabb() const -> std::tuple<double, double, double, double> /* min_x, min_y, max_x, max_y */ {
        double min_x = std::numeric_limits<double>::max();
        double min_y = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::min();
        double max_y = std::numeric_limits<double>::min();
        for (const auto& point : corner_points()) {
            min_x = std::min(min_x, point.x());
            min_y = std::min(min_y, point.y());
            max_x = std::max(max_x, point.x());
            max_y = std::max(max_y, point.y());
        }
        return std::make_tuple(min_x, min_y, max_x, max_y);
    }
    auto real_center() const -> Pose { return Pose((fl().x() + br().x()) / 2, (fl().y() + br().y()) / 2, center().theta()); }

    auto overlap_aabb(const Box& other) const -> bool {
        const auto [min_x1, min_y1, max_x1, max_y1] = aabb();
        const auto [min_x2, min_y2, max_x2, max_y2] = other.aabb();
        return (min_x1 <= max_x2) and (max_x1 >= min_x2) and (min_y1 <= max_y2) and (max_y1 >= min_y2);
    }

    auto overlap_obb(const Box& other) const -> bool {
        auto corners1 = corner_points();
        auto corners2 = other.corner_points();
        std::vector<Vec2D> axes;
        for (int i = 0; i < 4; ++i) {
            Vec2D edge = corners1[(i + 1) % 4] - corners1[i];
            axes.emplace_back(-edge.y(), edge.x());
        }
        for (int i = 0; i < 4; ++i) {
            Vec2D edge = corners2[(i + 1) % 4] - corners2[i];
            axes.emplace_back(-edge.y(), edge.x());
        }
        for (const auto& axis : axes) {
            double min1 = std::numeric_limits<double>::max();
            double max1 = std::numeric_limits<double>::lowest();
            double min2 = std::numeric_limits<double>::max();
            double max2 = std::numeric_limits<double>::lowest();
            for (const auto& corner : corners1) {
                double projection = corner.dot(axis);
                min1 = std::min(min1, projection);
                max1 = std::max(max1, projection);
            }
            for (const auto& corner : corners2) {
                double projection = corner.dot(axis);
                min2 = std::min(min2, projection);
                max2 = std::max(max2, projection);
            }
            if (max1 < min2  or  max2 < min1) {
                return false;
            }
        }
        return true;
    }

    auto overlap(const Box& other) const -> bool { return overlap_aabb(other) and overlap_obb(other); }
    auto overlap(const Pose& point) const -> bool { return overlap_aabb(Box(point, 0, 0, 0)) and overlap_obb(Box(point, 0, 0, 0)); }
};

}  // namespace star::base

#endif  // __BOX_H__

#ifndef __OBB_H__
#define __OBB_H__

#include <vector>
#include <cmath>
#include <algorithm>
#include "libraries/element/type/shape/vec2d.h"

namespace star::common::math {

class OBB2D {
  public:
    star::base::Vec2D center;
    star::base::Vec2D half_extents;
    double angle = 0.0;

    OBB2D() = default;
    OBB2D(const star::base::Vec2D& c, const star::base::Vec2D& he, double a)
        : center(c), half_extents(he), angle(a) {}

    auto get_corners() const -> std::vector<star::base::Vec2D> {
        std::vector<star::base::Vec2D> corners;
        double cos_a = std::cos(angle);
        double sin_a = std::sin(angle);
        corners.emplace_back(
            center.x() - half_extents.x() * cos_a - half_extents.y() * sin_a,
            center.y() - half_extents.x() * sin_a + half_extents.y() * cos_a
        );
        corners.emplace_back(
            center.x() + half_extents.x() * cos_a - half_extents.y() * sin_a,
            center.y() + half_extents.x() * sin_a + half_extents.y() * cos_a
        );
        corners.emplace_back(
            center.x() + half_extents.x() * cos_a + half_extents.y() * sin_a,
            center.y() + half_extents.x() * sin_a - half_extents.y() * cos_a
        );
        corners.emplace_back(
            center.x() - half_extents.x() * cos_a + half_extents.y() * sin_a,
            center.y() - half_extents.x() * sin_a - half_extents.y() * cos_a
        );
        return corners;
    }

    auto get_axes() const -> std::vector<star::base::Vec2D> {
        std::vector<star::base::Vec2D> axes;
        double cos_a = std::cos(angle);
        double sin_a = std::sin(angle);
        axes.emplace_back(cos_a, sin_a);
        axes.emplace_back(-sin_a, cos_a);
        return axes;
    }

    auto project(const star::base::Vec2D& axis) const -> std::pair<double, double> {
        auto corners = get_corners();
        double min_proj = std::numeric_limits<double>::infinity();
        double max_proj = -std::numeric_limits<double>::infinity();
        for (const auto& corner : corners) {
            double proj = corner * axis;
            min_proj = std::min(min_proj, proj);
            max_proj = std::max(max_proj, proj);
        }
        return {min_proj, max_proj};
    }

    auto intersects(const OBB2D& other, double epsilon = 1e-10) const -> bool {
        auto axes1 = get_axes();
        auto axes2 = other.get_axes();
        std::vector<star::base::Vec2D> all_axes = axes1;
        all_axes.insert(all_axes.end(), axes2.begin(), axes2.end());
        for (const auto& axis : all_axes) {
            auto proj1 = project(axis);
            auto proj2 = other.project(axis);
            if (proj1.second < proj2.first - epsilon  or  proj2.second < proj1.first - epsilon) {
                return false;
            }
        }
        return true;
    }
};

} // namespace star::common::math

#endif // __OBB_H__

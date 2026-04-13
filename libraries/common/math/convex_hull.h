#ifndef __CONVEX_HULL_H__
#define __CONVEX_HULL_H__

#include <vector>
#include <algorithm>
#include "libraries/element/type/shape/vec2d.h"

namespace star::common::math {

class ConvexHull {
  public:
    static auto compute(std::vector<star::base::Vec2D> points) -> std::vector<star::base::Vec2D> {
        if (points.size() <= 1) {
            return points;
        }
        std::sort(points.begin(), points.end(), [](const auto& a, const auto& b) {
            return a.x() < b.x()  or  (a.x() == b.x()  and  a.y() < b.y());
        });
        std::vector<star::base::Vec2D> hull;
        size_t n = points.size();
        size_t k = 0;
        hull.resize(2 * n);
        for (size_t i = 0; i < n; ++i) {
            while (k >= 2  and  cross(hull[k-1] - hull[k-2], points[i] - hull[k-2]) <= 0) {
                --k;
            }
            hull[k++] = points[i];
        }
        for (size_t i = n - 2, t = k + 1; i > 0; --i) {
            while (k >= t  and  cross(hull[k-1] - hull[k-2], points[i] - hull[k-2]) <= 0) {
                --k;
            }
            hull[k++] = points[i];
        }
        hull.resize(k - 1);
        return hull;
    }

  private:
    static auto cross(const star::base::Vec2D& a, const star::base::Vec2D& b) -> double {
        return a.x() * b.y() - a.y() * b.x();
    }
};

} // namespace star::common::math

#endif // __CONVEX_HULL_H__

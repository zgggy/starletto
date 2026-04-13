#ifndef __GEOMETRY_H__
#define __GEOMETRY_H__

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "libraries/element/type/shape/vec2d.h"

namespace star::common::math {

class Geometry {
  public:
    static auto point_to_segment_distance(const star::base::Vec2D& p, const star::base::Vec2D& a, const star::base::Vec2D& b) -> double {
        star::base::Vec2D ab = b - a;
        star::base::Vec2D ap = p - a;
        double dot = ap * ab;
        double len_sq = ab.x() * ab.x() + ab.y() * ab.y();
        double t = std::max(0.0, std::min(1.0, dot / len_sq));
        star::base::Vec2D projection(a.x() + t * ab.x(), a.y() + t * ab.y());
        return p.distance_with(projection);
    }

    static auto point_in_polygon(const star::base::Vec2D& p, const std::vector<star::base::Vec2D>& polygon) -> bool {
        int n = static_cast<int>(polygon.size());
        if (n < 3) return false;
        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            if (((polygon[i].y() > p.y()) != (polygon[j].y() > p.y()))  and  (p.x() < (polygon[j].x() - polygon[i].x()) * (p.y() - polygon[i].y()) / (polygon[j].y() - polygon[i].y()) + polygon[i].x())) {
                inside = not inside;
            }
        }
        return inside;
    }

    static auto polygon_area(const std::vector<star::base::Vec2D>& polygon) -> double {
        double area = 0.0;
        int n = static_cast<int>(polygon.size());
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            area += polygon[i].x() * polygon[j].y();
            area -= polygon[j].x() * polygon[i].y();
        }
        return std::fabs(area) / 2.0;
    }

    static auto segment_intersection(const star::base::Vec2D& a1, const star::base::Vec2D& a2, const star::base::Vec2D& b1, const star::base::Vec2D& b2,
                                     double epsilon = 1e-10) -> std::optional<star::base::Vec2D> {
        star::base::Vec2D d1 = a2 - a1;
        star::base::Vec2D d2 = b2 - b1;
        star::base::Vec2D d = a1 - b1;
        double cross = d1.x() * d2.y() - d1.y() * d2.x();
        if (std::fabs(cross) < epsilon) {
            return std::nullopt;
        }
        double t = (d2.x() * d.y() - d2.y() * d.x()) / cross;
        double u = (d1.x() * d.y() - d1.y() * d.x()) / cross;
        if (t >= -epsilon  and  t <= 1.0 + epsilon  and  u >= -epsilon  and  u <= 1.0 + epsilon) {
            return star::base::Vec2D(a1.x() + t * d1.x(), a1.y() + t * d1.y());
        }
        return std::nullopt;
    }

    static auto polygon_centroid(const std::vector<star::base::Vec2D>& polygon) -> star::base::Vec2D {
        double cx = 0.0, cy = 0.0;
        double area = 0.0;
        int n = static_cast<int>(polygon.size());
        for (int i = 0; i < n; ++i) {
            int j = (i + 1) % n;
            double cross = polygon[i].x() * polygon[j].y() - polygon[j].x() * polygon[i].y();
            cx += (polygon[i].x() + polygon[j].x()) * cross;
            cy += (polygon[i].y() + polygon[j].y()) * cross;
            area += cross;
        }
        area *= 0.5;
        if (std::fabs(area) < 1e-10) {
            return star::base::Vec2D(0.0, 0.0);
        }
        cx /= (6.0 * area);
        cy /= (6.0 * area);
        return star::base::Vec2D(cx, cy);
    }
};

}  // namespace star::common::math

#endif  // __GEOMETRY_H__

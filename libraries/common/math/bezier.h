#ifndef __BEZIER_H__
#define __BEZIER_H__

#include <cmath>
#include <stdexcept>
#include <vector>

#include "libraries/element/type/shape/vec2d.h"

namespace star::common::math {

class Bezier {
  public:
    static auto evaluate(const std::vector<star::base::Vec2D>& control_points, double t) -> star::base::Vec2D {
        if (control_points.empty()) {
            throw std::invalid_argument("Control points cannot be empty");
        }
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        std::vector<star::base::Vec2D> points = control_points;
        size_t n = points.size();
        for (size_t k = 1; k < n; ++k) {
            for (size_t i = 0; i < n - k; ++i) {
                points[i] = star::base::Vec2D((1.0 - t) * points[i].x() + t * points[i + 1].x(), (1.0 - t) * points[i].y() + t * points[i + 1].y());
            }
        }
        return points[0];
    }

    static auto sample(const std::vector<star::base::Vec2D>& control_points, size_t num_samples) -> std::vector<star::base::Vec2D> {
        std::vector<star::base::Vec2D> result;
        result.reserve(num_samples);
        double dt = 1.0 / static_cast<double>(num_samples - 1);
        for (size_t i = 0; i < num_samples; ++i) {
            double t = static_cast<double>(i) * dt;
            result.push_back(evaluate(control_points, t));
        }
        return result;
    }

    static auto derivative(const std::vector<star::base::Vec2D>& control_points) -> std::vector<star::base::Vec2D> {
        size_t n = control_points.size();
        if (n <= 1) {
            return {};
        }
        std::vector<star::base::Vec2D> derivative_points;
        derivative_points.reserve(n - 1);
        double scale = static_cast<double>(n - 1);
        for (size_t i = 0; i < n - 1; ++i) {
            derivative_points.emplace_back(scale * (control_points[i + 1].x() - control_points[i].x()), scale * (control_points[i + 1].y() - control_points[i].y()));
        }
        return derivative_points;
    }

    static auto length(const std::vector<star::base::Vec2D>& control_points, size_t num_segments = 100) -> double {
        if (control_points.size() <= 1) {
            return 0.0;
        }
        double total_length = 0.0;
        star::base::Vec2D prev = control_points[0];
        auto sampled = sample(control_points, num_segments + 1);
        for (size_t i = 1; i < sampled.size(); ++i) {
            total_length += sampled[i].distance_with(sampled[i - 1]);
        }
        return total_length;
    }

    static auto cubic(const star::base::Vec2D& p0, const star::base::Vec2D& p1, const star::base::Vec2D& p2, const star::base::Vec2D& p3, double t) -> star::base::Vec2D {
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double t2 = t * t;
        double t3 = t2 * t;
        double mt = 1.0 - t;
        double mt2 = mt * mt;
        double mt3 = mt2 * mt;
        return star::base::Vec2D(mt3 * p0.x() + 3.0 * mt2 * t * p1.x() + 3.0 * mt * t2 * p2.x() + t3 * p3.x(), mt3 * p0.y() + 3.0 * mt2 * t * p1.y() + 3.0 * mt * t2 * p2.y() + t3 * p3.y());
    }

    static auto quadratic(const star::base::Vec2D& p0, const star::base::Vec2D& p1, const star::base::Vec2D& p2, double t) -> star::base::Vec2D {
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        double mt = 1.0 - t;
        double mt2 = mt * mt;
        double t2 = t * t;
        return star::base::Vec2D(mt2 * p0.x() + 2.0 * mt * t * p1.x() + t2 * p2.x(), mt2 * p0.y() + 2.0 * mt * t * p1.y() + t2 * p2.y());
    }
};

}  // namespace star::common::math

#endif  // __BEZIER_H__

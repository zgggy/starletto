#ifndef __MAP_GEOMETRY_H__
#define __MAP_GEOMETRY_H__

#include <string>
#include <vector>
#include "libraries/element/type/shape/vec2d.h"

namespace star::element::type::map {

struct Point3D {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::string boundary_type = "hard";
    Point3D() = default;
    Point3D(double x, double y, double z) : x(x), y(y), z(z) {}
    Point3D(double x, double y, double z, const std::string& bt)
        : x(x), y(y), z(z), boundary_type(bt) {}
};

struct QuadraticPolynomial {
    double a0 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double a3 = 0.0;
    double a4 = 0.0;
    double a5 = 0.0;
    auto eval(double s) const -> double {
        return a0 + a1 * s + a2 * s * s + a3 * s * s * s +
               a4 * s * s * s * s + a5 * s * s * s * s * s;
    }
};

struct SpiralSegment {
    double x = 0.0;
    double y = 0.0;
    double theta = 0.0;
    double kappa = 0.0;
    double dkappa = 0.0;
    double length = 0.0;
    SpiralSegment() = default;
    SpiralSegment(double x, double y, double theta, double kappa,
                  double dkappa, double length)
        : x(x), y(y), theta(theta), kappa(kappa),
          dkappa(dkappa), length(length) {}
};

struct Polygon {
    std::vector<Point3D> points;
};

struct BlockedPolygon {
    std::string id;
    std::vector<Point3D> points;
};

struct Portal {
    std::string id;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::string direction = "bidirectional";
};

struct DrivableAreaGeometry {
    Polygon raw_polygon;
    std::vector<BlockedPolygon> blocked_polygons;
};

} // namespace star::element::type::map

#endif // __MAP_GEOMETRY_H__

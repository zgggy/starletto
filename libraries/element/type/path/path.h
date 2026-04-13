#ifndef __PATH_H__
#define __PATH_H__

#include <vector>

#include "element/type/shape/point.h"
#include "element/type/shape/vec2d.h"

namespace star::base {

class Path : public std::vector<PathPoint> {
  public:
    Path() = default;
    Path(const std::vector<PathPoint>& points) : std::vector<PathPoint>(points) {}
    Path(const Path& other) : std::vector<PathPoint>(other) {}

    auto length() const -> double { return empty() ? 0.0 : back().s() - front().s(); }

    auto str() const -> std::string {
        std::string result = "Path: [";
        for (const auto& point : *this) {
            result += point.str() + ", ";
        }
        result += "]";
        return result;
    }
};

}  // namespace star::base

#endif  // __PATH_H__

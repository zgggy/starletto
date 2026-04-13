#ifndef __MAP_META_H__
#define __MAP_META_H__

#include <any>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

namespace star::element::type::map {

struct CoordinateSystem {
    std::string type = "utm";
    int zone = 50;
    std::string hemisphere = "north";
    std::string datum = "WGS84";
};

struct Origin {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double theta = 0.0;
};

struct SRanges {
    double min = 0.0;
    double max = 0.0;
    SRanges() = default;
    SRanges(double min, double max) : min(min), max(max) {}
};

struct ZRange {
    double min = -0.5;
    double max = 3.5;
    ZRange() = default;
    ZRange(double min, double max) : min(min), max(max) {}
};

struct ValidPeriod {
    int64_t start = 0;
    int64_t end = 4102444800;
    ValidPeriod() = default;
    ValidPeriod(int64_t start, int64_t end) : start(start), end(end) {}
};

struct SpeedLimit {
    double max = 30.0;
    double min = 5.0;
    SpeedLimit() = default;
    SpeedLimit(double max, double min) : max(max), min(min) {}
};

struct IncrementInfo {
    std::string base_id;
    std::string increment_version;
    double x_min = 0.0;
    double x_max = 0.0;
    double y_min = 0.0;
    double y_max = 0.0;
};

struct Header {
    std::string id;
    std::string name;
    std::string version = "4.0.0";
    CoordinateSystem coordinate_system;
    Origin origin;
    std::unordered_map<std::string, std::vector<std::string>> relation;
    std::optional<IncrementInfo> increment_info;
    int64_t timestamp = 0;
    std::unordered_map<std::string, std::any> custom_properties;
};

struct Layer {
    std::string id;
    std::string name;
    std::string type;
    ZRange z_range;
    ValidPeriod valid_period;
    bool is_enabled = true;
    std::unordered_map<std::string, std::vector<std::string>> relation;
    std::unordered_map<std::string, std::any> custom_properties;
};

}  // namespace star::element::type::map

#endif  // __MAP_META_H__

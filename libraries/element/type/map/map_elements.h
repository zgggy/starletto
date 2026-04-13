#ifndef __MAP_ELEMENTS_H__
#define __MAP_ELEMENTS_H__

#include <any>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "libraries/element/type/map/map_geometry.h"
#include "libraries/element/type/map/map_meta.h"

namespace star::element::type::map {

struct DrivableArea {
    std::string id;
    std::string name;
    std::string layer;
    DrivableAreaGeometry geometry;
    std::vector<Portal> portals;
    SpeedLimit default_speed_limit;
    bool is_enabled = true;
    std::unordered_map<std::string, std::vector<std::string>> relation;
    std::unordered_map<std::string, std::any> custom_properties;
};

struct RoadReferenceLine {
    std::string id;
    std::string name;
    std::string layer;
    std::vector<SpiralSegment> spiral_segments;
    SRanges s_range;
    bool is_relative = false;
    bool is_enabled = true;
    std::unordered_map<std::string, std::vector<std::string>> relation;
    std::unordered_map<std::string, std::any> custom_properties;
};

struct MapElement {
    std::string id;
    std::string name;
    std::string type;
    std::string layer;
    Polygon geometry;
    std::unordered_map<std::string, std::any> properties;
    ValidPeriod valid_period;
    bool is_enabled = true;
    std::unordered_map<std::string, std::vector<std::string>> relation;
    std::unordered_map<std::string, std::any> custom_properties;
};

struct SoftBoundaryPolynomials {
    QuadraticPolynomial left;
    QuadraticPolynomial right;
};

struct Lane {
    std::string id;
    std::string name;
    std::string type;
    std::string layer;
    std::string bind_rrl;
    std::string bind_vrl;
    SRanges s_range;
    SoftBoundaryPolynomials soft_boundary_polynomials;
    std::string direction = "forward";
    SpeedLimit speed_limit;
    bool is_enabled = true;
    std::unordered_map<std::string, std::vector<std::any>> relation;
    std::unordered_map<std::string, std::any> custom_properties;
};

struct VirtualReferenceLineNode {
    std::string id;
    std::string type;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    std::string bind_lane;
};

struct VirtualReferenceLineEdge {
    std::string from;
    std::string to;
    double weight = 0.0;
    bool is_bidirectional = true;
};

struct VirtualReferenceLine {
    std::string id;
    std::string name;
    std::string layer;
    std::vector<VirtualReferenceLineNode> nodes;
    std::vector<VirtualReferenceLineEdge> edges;
};

struct RoutingGraphNode {
    std::string id;
    std::string type;
    std::string layer;
};

struct RoutingGraphEdge {
    std::string id;
    std::string from;
    std::string to;
    double weight = 0.0;
    std::string type;
    bool is_bidirectional = false;
};

struct RoutingGraph {
    std::vector<VirtualReferenceLine> virtual_reference_lines;
    std::vector<RoutingGraphNode> nodes;
    std::vector<RoutingGraphEdge> edges;
};

struct URMap {
    Header header;
    std::vector<Layer> layers;
    std::vector<DrivableArea> drivable_areas;
    std::vector<RoadReferenceLine> road_reference_lines;
    std::vector<MapElement> map_elements;
    std::vector<Lane> lanes;
    std::optional<RoutingGraph> routing_graph;
    std::unordered_map<std::string, std::any> custom_properties;
};

}  // namespace star::element::type::map

#endif  // __MAP_ELEMENTS_H__

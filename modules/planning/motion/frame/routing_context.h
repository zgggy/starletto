#ifndef __ROUTING_CONTEXT_H__
#define __ROUTING_CONTEXT_H__

namespace star::planning::data {

struct RoutingContext {
    bool routing_updated = false;
    // std::vector<std::string> routing_path;
};

} // namespace star::planning::data

#endif // __ROUTING_CONTEXT_H__

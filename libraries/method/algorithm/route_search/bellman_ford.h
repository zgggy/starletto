#ifndef __BELLMAN_FORD_H__
#define __BELLMAN_FORD_H__

#include "basement/core/logger/logger.h"
#include "route_search.h"

namespace star::methods {

class BellmanFord : public RouteSearchAlgorithm {
  public:
    BellmanFord(const Graph& graph) : RouteSearchAlgorithm(graph) {}

    auto search(int start, int end) -> std::pair<std::vector<int>, int> override {
        std::unordered_map<int, int> distances;
        std::unordered_map<int, int> previous;

        for (const auto& node : graph.adjList) {
            distances[node.first] = std::numeric_limits<int>::max();
        }
        distances[start] = 0;

        for (size_t i = 0; i < graph.adjList.size() - 1; ++i) {
            for (const auto& node : graph.adjList) {
                for (const auto& neighbor : node.second) {
                    int u = node.first;
                    int v = neighbor.first;
                    int weight = neighbor.second;
                    if (distances[u] != std::numeric_limits<int>::max()  and  distances[u] + weight < distances[v]) {
                        distances[v] = distances[u] + weight;
                        previous[v] = u;
                    }
                }
            }
        }

        for (const auto& node : graph.adjList) {
            for (const auto& neighbor : node.second) {
                int u = node.first;
                int v = neighbor.first;
                int weight = neighbor.second;
                if (distances[u] != std::numeric_limits<int>::max()  and  distances[u] + weight < distances[v]) {
                    logE << "Graph contains a negative weight cycle";
                    return {{}, std::numeric_limits<int>::max()};
                }
            }
        }

        std::vector<int> path;
        for (int at = end; at != -1; at = previous[at]) {
            path.push_back(at);
        }
        std::reverse(path.begin(), path.end());

        if (path.empty()  or  path[0] != start) {
            return {{}, std::numeric_limits<int>::max()};
        }

        return {path, distances[end]};
    }
};

}  // namespace star::methods

#endif  // __BELLMAN_FORD_H__

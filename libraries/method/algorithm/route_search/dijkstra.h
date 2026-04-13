#ifndef __DIJKSTRA_H__
#define __DIJKSTRA_H__

#include "route_search.h"

namespace star::methods {

class Dijkstra : public RouteSearchAlgorithm {
  public:
    Dijkstra(const Graph& graph) : RouteSearchAlgorithm(graph) {}

    auto search(int start, int end) -> std::pair<std::vector<int>, int> override {
        std::unordered_map<int, int> distances;
        std::unordered_map<int, int> previous;
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

        for (const auto& node : graph.adjList) {
            distances[node.first] = std::numeric_limits<int>::max();
        }
        distances[start] = 0;
        pq.push({0, start});

        while (not pq.empty()) {
            int currentDist = pq.top().first;
            int currentNode = pq.top().second;
            pq.pop();

            if (currentNode == end) {
                std::vector<int> path;
                for (int at = end; at != -1; at = previous[at]) {
                    path.push_back(at);
                }
                std::reverse(path.begin(), path.end());
                return {path, currentDist};
            }

            if (currentDist > distances[currentNode]) {
                continue;
            }

            for (const auto& neighbor : graph.adjList.at(currentNode)) {
                int neighborNode = neighbor.first;
                int weight = neighbor.second;
                int distance = currentDist + weight;

                if (distance < distances[neighborNode]) {
                    distances[neighborNode] = distance;
                    previous[neighborNode] = currentNode;
                    pq.push({distance, neighborNode});
                }
            }
        }

        return {{}, std::numeric_limits<int>::max()};
    }
};

}  // namespace star::methods

#endif  // __DIJKSTRA_H__
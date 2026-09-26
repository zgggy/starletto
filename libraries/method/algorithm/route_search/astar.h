#ifndef __ASTAR_H__
#define __ASTAR_H__

#include "route_search.h"

namespace star::methods {

class AStar : public RouteSearchAlgorithm {
  public:
    AStar(const Graph& graph) : RouteSearchAlgorithm(graph) {}

    auto search(int start, int end) -> std::pair<std::vector<int>, int> override {
        std::unordered_map<int, int> gScore;
        std::unordered_map<int, int> fScore;
        std::unordered_map<int, int> cameFrom;
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> openSet;

        for (const auto& node : graph.adjList) {
            gScore[node.first] = std::numeric_limits<int>::max();
            fScore[node.first] = std::numeric_limits<int>::max();
        }
        gScore[start] = 0;
        fScore[start] = heuristic(start, end);
        openSet.push({fScore[start], start});

        while (not openSet.empty()) {
            int currentNode = openSet.top().second;
            openSet.pop();

            if (currentNode == end) {
                std::vector<int> path;
                for (int at = end; at != -1; at = cameFrom[at]) {
                    path.push_back(at);
                }
                std::reverse(path.begin(), path.end());
                return {path, gScore[end]};
            }

            for (const auto& neighbor : graph.adjList.at(currentNode)) {
                int neighborNode = neighbor.first;
                int tentativeGScore = gScore[currentNode] + neighbor.second;

                if (tentativeGScore < gScore[neighborNode]) {
                    cameFrom[neighborNode] = currentNode;
                    gScore[neighborNode] = tentativeGScore;
                    fScore[neighborNode] = tentativeGScore + heuristic(neighborNode, end);
                    openSet.push({fScore[neighborNode], neighborNode});
                }
            }
        }

        return {{}, std::numeric_limits<int>::max()};
    }

  private:
    auto heuristic(int a, int b) -> int {
        // 简单示例，可根据实际情况修改
        return 0;
    }
};

}  // namespace star::methods

#endif  // __ASTAR_H__

// int main() {
//     Graph graph;
//     graph.addNode(0);
//     graph.addNode(1);
//     graph.addNode(2);
//     graph.addEdge(0, 1, 1);
//     graph.addEdge(1, 2, 2);
//     graph.addEdge(0, 2, 4);

//     Dijkstra dijkstra(graph);
//     auto [dijkstraPath, dijkstraDistance] = dijkstra.search(0, 2);
//     std::cout << "Dijkstra Path: ";
//     for (int node : dijkstraPath) {
//         std::cout << node << " ";
//     }
//     std::cout << std::endl;
//     std::cout << "Dijkstra Distance: " << dijkstraDistance << std::endl;

//     AStar aStar(graph);
//     auto [aStarPath, aStarDistance] = aStar.search(0, 2);
//     std::cout << "A* Path: ";
//     for (int node : aStarPath) {
//         std::cout << node << " ";
//     }
//     std::cout << std::endl;
//     std::cout << "A* Distance: " << aStarDistance << std::endl;

//     BellmanFord bellmanFord(graph);
//     auto [bellmanFordPath, bellmanFordDistance] = bellmanFord.search(0, 2);
//     std::cout << "Bellman - Ford Path: ";
//     for (int node : bellmanFordPath) {
//         std::cout << node << " ";
//     }
//     std::cout << std::endl;
//     std::cout << "Bellman - Ford Distance: " << bellmanFordDistance << std::endl;

//     return 0;
// }
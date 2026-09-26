#ifndef __ROUTE_SEARCH_H__
#define __ROUTE_SEARCH_H__

#include <algorithm>
#include <iostream>
#include <limits>
#include <queue>
#include <unordered_map>
#include <vector>

namespace star::methods {

class Graph {
  public:
    void add_node(int node) { adjList[node] = {}; }

    void add_edge(int from, int to, int weight) { adjList[from][to] = weight; }

    std::unordered_map<int, std::unordered_map<int, int>> adjList;
};

class RouteSearchAlgorithm {
  public:
    RouteSearchAlgorithm(const Graph& graph) : graph(graph) {}
    virtual auto search(int start, int end) -> std::pair<std::vector<int>, int> = 0;

  protected:
    const Graph& graph;
};

}  // namespace star::methods

#endif  // __ROUTE_SEARCH_H__

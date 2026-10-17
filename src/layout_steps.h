// layout_steps.h (private) - the stages of the layered layout, one class each.
#pragma once

#include <utility>
#include <vector>

#include "layered_graph.h"

namespace mermaid::flowchart::detail {

struct RankEdge {
    int from;
    int to;
    int min_length;
};

// Finds edges to flip so the graph becomes acyclic (depth-first search).
class CycleBreaker {
public:
    std::vector<bool> reversed(int node_count, const std::vector<std::pair<int, int>>& edges) const;
};

// Longest-path ranking; sources are then pulled down next to their targets.
class Ranker {
public:
    std::vector<int> rank(int node_count, const std::vector<RankEdge>& edges) const;
};

// Orders each layer: barycenter sweeps, keeping subgraph members together.
class CrossingMinimizer {
public:
    void run(LayeredGraph& graph) const;
};

struct Spacing {
    double node_sep = 40;
    double edge_sep = 20;
    double rank_sep = 40;
    double cluster_pad = 8;
    std::vector<double> extra_gap;  // extra room between layer i and i + 1
};

// Gives every vertex its centre: y from the layer heights, x by relaxing towards
// the neighbours while keeping the order and the minimum distances.
class CoordinateAssigner {
public:
    explicit CoordinateAssigner(Spacing spacing) : spacing_(std::move(spacing)) {}
    void assign(LayeredGraph& graph) const;

private:
    double separation(const Vertex& a, const Vertex& b) const;
    Spacing spacing_;
};

// Turns chains into polylines (flow orientation). Edge ends are spread along the
// side of their node; bends sit on the layer boundaries.
class EdgeRouter {
public:
    explicit EdgeRouter(double rank_sep) : rank_sep_(rank_sep) {}
    std::vector<std::vector<Point>> route(const LayeredGraph& graph) const;

private:
    double rank_sep_;
};

}  // namespace mermaid::flowchart::detail

// dagre_util.h (private) - helpers shared by the layout stages.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "dagre_graph.h"

namespace mermaid::dagre::detail {

// Hands out the names of dummy nodes and reversed edges ("_d12", "rev3", ...).
class IdGenerator {
public:
    std::string next(const std::string& prefix) { return prefix + std::to_string(++counter_); }

private:
    long counter_ = 0;
};

using NodeLabelPtr = std::shared_ptr<NodeLabel>;
using EdgeLabelPtr = std::shared_ptr<EdgeLabel>;
using Layers = std::vector<std::vector<std::string>>;

// A new dummy node of the given kind; returns its id.
std::string add_dummy_node(LayoutGraph& g, IdGenerator& ids, Dummy kind, NodeLabelPtr label,
                           const std::string& prefix);

// A new border dummy node; with `rank` set it is also placed in that rank.
std::string add_border_node(LayoutGraph& g, IdGenerator& ids, const std::string& prefix,
                            std::optional<int> rank = std::nullopt, std::optional<int> order = std::nullopt);

// Same nodes (labels shared), parallel edges merged into one (weights add, minlen is the max).
LayoutGraph simplify(const LayoutGraph& g);

// Only the nodes without children, and all edges; labels are shared with `g`.
LayoutGraph as_non_compound_graph(const LayoutGraph& g);

// Highest rank of any ranked node, or nothing when no node has a rank.
std::optional<int> max_rank(const LayoutGraph& g);

// layers[rank][order] = node id.
Layers build_layer_matrix(const LayoutGraph& g);

void normalize_ranks(LayoutGraph& g);
void remove_empty_ranks(LayoutGraph& g);

// Where the line from the centre of `rect` towards `point` leaves the rectangle.
Point intersect_rect(const NodeLabel& rect, const Point& point);

}  // namespace mermaid::dagre::detail

#include "dagre_util.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mermaid::dagre::detail {

std::string add_dummy_node(LayoutGraph& g, IdGenerator& ids, Dummy kind, NodeLabelPtr label,
                           const std::string& prefix) {
    std::string v;
    do {
        v = ids.next(prefix);
    } while (g.has_node(v));
    label->dummy = kind;
    g.set_node(v, std::move(label));
    return v;
}

std::string add_border_node(LayoutGraph& g, IdGenerator& ids, const std::string& prefix,
                            std::optional<int> rank, std::optional<int> order) {
    auto node = std::make_shared<NodeLabel>();
    if (rank) {
        node->rank = *rank;
        node->order = order;
    }
    return add_dummy_node(g, ids, Dummy::Border, std::move(node), prefix);
}

LayoutGraph simplify(const LayoutGraph& g) {
    LayoutGraph simplified(true, false, false);
    simplified.set_graph(g.graph_ptr());
    for (const std::string& v : g.nodes()) simplified.set_node(v, g.node(v));
    for (const EdgeObj& e : g.edges()) {
        const EdgeLabelPtr previous = simplified.edge(e.v, e.w);
        const double weight = previous ? previous->weight : 0;
        const double minlen = previous ? previous->minlen : 1;
        const EdgeLabelPtr label = g.edge(e);
        auto merged = std::make_shared<EdgeLabel>();
        merged->weight = weight + label->weight;
        merged->minlen = std::max(minlen, label->minlen);
        simplified.set_edge(e.v, e.w, merged);
    }
    return simplified;
}

LayoutGraph as_non_compound_graph(const LayoutGraph& g) {
    LayoutGraph simplified(true, g.is_multigraph(), false);
    simplified.set_graph(g.graph_ptr());
    for (const std::string& v : g.nodes())
        if (!g.has_children(v)) simplified.set_node(v, g.node(v));
    for (const EdgeObj& e : g.edges()) simplified.set_edge(e, g.edge(e));
    return simplified;
}

std::optional<int> max_rank(const LayoutGraph& g) {
    std::optional<double> best;
    for (const std::string& v : g.nodes()) {
        const auto& rank = g.node(v)->rank;
        if (rank && (!best || *rank > *best)) best = rank;
    }
    if (!best) return std::nullopt;
    return static_cast<int>(*best);
}

Layers build_layer_matrix(const LayoutGraph& g) {
    const std::optional<int> top = max_rank(g);
    if (!top) return {};
    Layers layering(static_cast<size_t>(*top) + 1);
    for (const std::string& v : g.nodes()) {
        const auto node = g.node(v);
        if (!node->rank) continue;
        auto& layer = layering[static_cast<size_t>(*node->rank)];
        const size_t order = static_cast<size_t>(node->order.value_or(0));
        if (layer.size() <= order) layer.resize(order + 1);
        layer[order] = v;
    }
    return layering;
}

void normalize_ranks(LayoutGraph& g) {
    double lowest = std::numeric_limits<double>::infinity();
    for (const std::string& v : g.nodes())
        if (const auto& rank = g.node(v)->rank) lowest = std::min(lowest, *rank);
    for (const std::string& v : g.nodes()) {
        const auto node = g.node(v);
        if (node->rank) *node->rank -= lowest;
    }
}

void remove_empty_ranks(LayoutGraph& g) {
    double offset = std::numeric_limits<double>::infinity();
    for (const std::string& v : g.nodes())
        if (const auto& rank = g.node(v)->rank) offset = std::min(offset, *rank);

    // A rank that is not a whole number never lands in a layer (as in dagre).
    std::vector<std::vector<std::string>> layers;
    std::vector<bool> present;
    for (const std::string& v : g.nodes()) {
        if (!g.node(v)->rank) continue;
        const double rank = *g.node(v)->rank - offset;
        if (rank < 0 || rank != std::floor(rank)) continue;
        const size_t index = static_cast<size_t>(rank);
        if (layers.size() <= index) {
            layers.resize(index + 1);
            present.resize(index + 1, false);
        }
        layers[index].push_back(v);
        present[index] = true;
    }

    double delta = 0;
    const double factor = g.graph().node_rank_factor;
    for (size_t i = 0; i < layers.size(); ++i) {
        if (!present[i] && std::fmod(static_cast<double>(i), factor) != 0) {
            --delta;
        } else if (delta != 0) {
            for (const std::string& v : layers[i]) *g.node(v)->rank += delta;
        }
    }
}

Point intersect_rect(const NodeLabel& rect, const Point& point) {
    const double x = *rect.x;
    const double y = *rect.y;
    const double dx = point.x - x;
    const double dy = point.y - y;
    double w = rect.width / 2;
    double h = rect.height / 2;
    if (dx == 0 && dy == 0) throw std::runtime_error("Not possible to find intersection inside of the rectangle");
    double sx;
    double sy;
    if (std::fabs(dy) * w > std::fabs(dx) * h) {
        if (dy < 0) h = -h;
        sx = (h * dx) / dy;
        sy = h;
    } else {
        if (dx < 0) w = -w;
        sx = w;
        sy = (w * dy) / dx;
    }
    return {x + sx, y + sy};
}

}  // namespace mermaid::dagre::detail

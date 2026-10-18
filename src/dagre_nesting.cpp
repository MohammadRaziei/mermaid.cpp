#include "dagre_nesting.h"

#include <functional>
#include <mermaid/macros.h>

namespace mermaid::dagre::detail {

void NestingGraph::run() {
    const std::string root = add_dummy_node(g_, ids_, Dummy::Root, std::make_shared<NodeLabel>(), "_root");

    std::map<std::string, int> depths;
    std::function<void(const std::string&, int)> measure = [&](const std::string& v, int depth) {
        for (const std::string& child : g_.children(v)) measure(child, depth + 1);
        depths[v] = depth;
    };
    for (const std::string& v : g_.children()) measure(v, 1);

    int deepest = 0;
    for (const auto& entry : depths) deepest = MERMAID_MAX(deepest, entry.second);
    const int height = deepest - 1;
    const double node_sep = 2.0 * height + 1;

    g_.graph().nesting_root = root;

    for (const EdgeObj& e : g_.edges()) g_.edge(e)->minlen *= node_sep;

    double weight = 1;
    for (const EdgeObj& e : g_.edges()) weight += g_.edge(e)->weight;

    for (const std::string& child : g_.children()) connect(root, node_sep, weight, height, depths, child);

    g_.graph().node_rank_factor = node_sep;
}

void NestingGraph::connect(const std::string& root, double node_sep, double weight, int height,
                           const std::map<std::string, int>& depths, const std::string& v) {
    const std::vector<std::string> children = g_.children(v);
    if (children.empty()) {
        if (v != root) {
            auto edge = std::make_shared<EdgeLabel>();
            edge->weight = 0;
            edge->minlen = node_sep;
            g_.set_edge(root, v, edge);
        }
        return;
    }

    const std::string top = add_border_node(g_, ids_, "_bt");
    const std::string bottom = add_border_node(g_, ids_, "_bb");
    const NodeLabelPtr label = g_.node(v);
    g_.set_parent(top, v);
    label->border_top = top;
    g_.set_parent(bottom, v);
    label->border_bottom = bottom;

    for (const std::string& child : children) {
        connect(root, node_sep, weight, height, depths, child);

        const NodeLabelPtr child_node = g_.node(child);
        const bool has_borders = !child_node->border_top.empty();
        const std::string child_top = has_borders ? child_node->border_top : child;
        const std::string child_bottom = has_borders ? child_node->border_bottom : child;
        const double this_weight = has_borders ? weight : 2 * weight;
        const double minlen = child_top != child_bottom ? 1 : height - depths.at(v) + 1;

        auto down = std::make_shared<EdgeLabel>();
        down->weight = this_weight;
        down->minlen = minlen;
        down->nesting_edge = true;
        g_.set_edge(top, child_top, down);

        auto up = std::make_shared<EdgeLabel>();
        up->weight = this_weight;
        up->minlen = minlen;
        up->nesting_edge = true;
        g_.set_edge(child_bottom, bottom, up);
    }

    if (g_.parent(v).empty()) {
        auto edge = std::make_shared<EdgeLabel>();
        edge->weight = 0;
        edge->minlen = height + depths.at(v);
        g_.set_edge(root, top, edge);
    }
}

void NestingGraph::cleanup() {
    GraphLabel& label = g_.graph();
    g_.remove_node(label.nesting_root);
    label.nesting_root.clear();
    for (const EdgeObj& e : g_.edges())
        if (g_.edge(e)->nesting_edge) g_.remove_edge(e);
}

}  // namespace mermaid::dagre::detail

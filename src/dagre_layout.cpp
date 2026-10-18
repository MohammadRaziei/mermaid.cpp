// dagre_layout.cpp - the public entry point: runs the dagre stages in order.
#include <mermaid/dagre.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "dagre_acyclic.h"
#include "dagre_nesting.h"
#include "dagre_normalize.h"
#include "dagre_order.h"
#include "dagre_position.h"
#include "dagre_rank.h"
#include "dagre_util.h"
#include <mermaid/macros.h>

namespace mermaid::dagre {

const NodeResult* Result::find_node(const std::string& id) const {
    for (const NodeResult& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

const EdgeResult* Result::find_edge(const std::string& v, const std::string& w, const std::string& name) const {
    for (const EdgeResult& e : edges)
        if (e.v == v && e.w == w && e.name == name) return &e;
    return nullptr;
}

namespace {

using namespace detail;

// Swaps width/height, x/y or flips y so that every direction is laid out as top-to-bottom.
class CoordinateSystem {
public:
    explicit CoordinateSystem(LayoutGraph& g) : g_(g) {}

    void adjust() {
        const RankDir dir = g_.graph().rankdir;
        if (dir == RankDir::LR || dir == RankDir::RL) swap_width_height();
    }
    void undo() {
        const RankDir dir = g_.graph().rankdir;
        if (dir == RankDir::BT || dir == RankDir::RL) reverse_y();
        if (dir == RankDir::LR || dir == RankDir::RL) {
            swap_xy();
            swap_width_height();
        }
    }

private:
    void swap_width_height() {
        for (const std::string& v : g_.nodes()) std::swap(g_.node(v)->width, g_.node(v)->height);
        for (const EdgeObj& e : g_.edges()) std::swap(g_.edge(e)->width, g_.edge(e)->height);
    }
    void reverse_y() {
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr n = g_.node(v);
            if (n->y) n->y = -*n->y;
        }
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (edge->points)
                for (Point& p : *edge->points) p.y = -p.y;
            if (edge->y) edge->y = -*edge->y;
        }
    }
    void swap_xy() {
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr n = g_.node(v);
            std::swap(n->x, n->y);
        }
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (edge->points)
                for (Point& p : *edge->points) std::swap(p.x, p.y);
            if (edge->x) std::swap(edge->x, edge->y);
        }
    }

    LayoutGraph& g_;
};

class LayoutPipeline {
public:
    explicit LayoutPipeline(const Input& input) : input_(input), g_(true, true, true) {}

    Result run() {
        build_layout_graph();
        run_layout();
        return collect_result();
    }

private:
    // ---- input -> layout graph

    void build_layout_graph() {
        auto label = std::make_shared<GraphLabel>();
        label->ranksep = input_.options.ranksep;
        label->edgesep = input_.options.edgesep;
        label->nodesep = input_.options.nodesep;
        label->marginx = input_.options.marginx;
        label->marginy = input_.options.marginy;
        label->rankdir = input_.options.rankdir;
        g_.set_graph(label);

        std::unordered_map<std::string, const Node*> by_id;
        std::vector<std::string> ids;
        for (const Node& n : input_.nodes) {
            if (!by_id.count(n.id)) ids.push_back(n.id);
            by_id[n.id] = &n;
        }
        for (const Node& n : input_.nodes)
            if (!n.parent.empty() && !by_id.count(n.parent))
                throw std::invalid_argument("dagre: unknown parent '" + n.parent + "' of node '" + n.id + "'");
        for (const Edge& e : input_.edges)
            if (!by_id.count(e.v) || !by_id.count(e.w))
                throw std::invalid_argument("dagre: edge " + e.v + " -> " + e.w + " uses an unknown node");

        // Same visiting order as a graphlib graph built from these nodes.
        for (const std::string& id : js_key_order(ids)) {
            const Node& n = *by_id.at(id);
            auto node = std::make_shared<NodeLabel>();
            node->width = n.width;
            node->height = n.height;
            g_.set_node(id, node);
            g_.set_parent(id, n.parent);
        }
        for (const Edge& e : input_.edges) {
            auto edge = std::make_shared<EdgeLabel>();
            edge->minlen = e.minlen;
            edge->weight = e.weight;
            edge->width = e.width;
            edge->height = e.height;
            edge->labeloffset = e.labeloffset;
            edge->labelpos = e.labelpos;
            g_.set_edge(e.v, e.w, edge, e.name);
        }
    }

    // ---- the stages, in dagre's order

    void run_layout() {
        make_space_for_edge_labels();
        remove_self_edges();
        Acyclic acyclic(g_, ids_);
        acyclic.run();
        NestingGraph nesting(g_, ids_);
        nesting.run();
        {
            const LayoutGraph flat = as_non_compound_graph(g_);
            NetworkSimplexRanker(flat).run();
        }
        inject_edge_label_proxies();
        remove_empty_ranks(g_);
        nesting.cleanup();
        normalize_ranks(g_);
        assign_rank_min_max();
        remove_edge_label_proxies();
        EdgeNormalizer normalizer(g_, ids_);
        normalizer.run();
        ParentDummyChains(g_).run();
        BorderSegments(g_, ids_).run();
        Orderer(g_, ids_).run();
        insert_self_edges();
        CoordinateSystem coordinates(g_);
        coordinates.adjust();
        position(g_);
        position_self_edges();
        remove_border_nodes();
        normalizer.undo();
        fixup_edge_label_coords();
        coordinates.undo();
        translate_graph();
        assign_node_intersects();
        reverse_points_for_reversed_edges();
        acyclic.undo();
    }

    // Labels get a rank of their own: halve the rank gap, double every edge length.
    void make_space_for_edge_labels() {
        GraphLabel& graph = g_.graph();
        graph.ranksep /= 2;
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            edge->minlen *= 2;
            if (edge->labelpos != LabelPos::Center) {
                if (graph.rankdir == RankDir::TB || graph.rankdir == RankDir::BT)
                    edge->width += edge->labeloffset;
                else
                    edge->height += edge->labeloffset;
            }
        }
    }

    void remove_self_edges() {
        for (const EdgeObj& e : g_.edges()) {
            if (e.v != e.w) continue;
            g_.node(e.v)->self_edges.push_back({e, g_.edge(e)});
            g_.remove_edge(e);
        }
    }

    // Remembers the rank an edge label should end up in.
    void inject_edge_label_proxies() {
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (edge->width == 0 || edge->height == 0) continue;
            const NodeLabelPtr v = g_.node(e.v);
            const NodeLabelPtr w = g_.node(e.w);
            auto proxy = std::make_shared<NodeLabel>();
            proxy->rank = (*w->rank - *v->rank) / 2 + *v->rank;
            proxy->edge_obj = e;
            add_dummy_node(g_, ids_, Dummy::EdgeProxy, proxy, "_ep");
        }
    }

    void assign_rank_min_max() {
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr node = g_.node(v);
            if (node->border_top.empty()) continue;
            node->min_rank = static_cast<int>(*g_.node(node->border_top)->rank);
            node->max_rank = static_cast<int>(*g_.node(node->border_bottom)->rank);
        }
    }

    void remove_edge_label_proxies() {
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr node = g_.node(v);
            if (node->dummy != Dummy::EdgeProxy) continue;
            g_.edge(node->edge_obj)->label_rank = node->rank;
            g_.remove_node(v);
        }
    }

    void insert_self_edges() {
        for (const auto& layer : build_layer_matrix(g_)) {
            int order_shift = 0;
            for (size_t i = 0; i < layer.size(); ++i) {
                const NodeLabelPtr node = g_.node(layer[i]);
                node->order = static_cast<int>(i) + order_shift;
                for (const SelfEdgeRec& self : node->self_edges) {
                    auto dummy = std::make_shared<NodeLabel>();
                    dummy->width = self.label->width;
                    dummy->height = self.label->height;
                    dummy->rank = node->rank;
                    dummy->order = static_cast<int>(i) + ++order_shift;
                    dummy->edge_obj = self.e;
                    dummy->edge_label = self.label;
                    add_dummy_node(g_, ids_, Dummy::SelfEdge, dummy, "_se");
                }
                node->self_edges.clear();
            }
        }
    }

    void position_self_edges() {
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr node = g_.node(v);
            if (node->dummy != Dummy::SelfEdge) continue;
            const NodeLabelPtr self = g_.node(node->edge_obj.v);
            const double x = *self->x + self->width / 2;
            const double y = *self->y;
            const double dx = *node->x - x;
            const double dy = self->height / 2;
            const EdgeLabelPtr label = node->edge_label;
            g_.set_edge(node->edge_obj, label);
            g_.remove_node(v);
            label->points = std::vector<Point>{{x + 2 * dx / 3, y - dy}, {x + 5 * dx / 6, y - dy}, {x + dx, y},
                                               {x + 5 * dx / 6, y + dy}, {x + 2 * dx / 3, y + dy}};
            label->x = node->x;
            label->y = node->y;
        }
    }

    // Compound nodes take their box from their border nodes.
    void remove_border_nodes() {
        for (const std::string& v : g_.nodes()) {
            if (!g_.has_children(v)) continue;
            const NodeLabelPtr node = g_.node(v);
            const NodeLabelPtr top = g_.node(node->border_top);
            const NodeLabelPtr bottom = g_.node(node->border_bottom);
            const NodeLabelPtr left = g_.node(node->border_left.rbegin()->second);
            const NodeLabelPtr right = g_.node(node->border_right.rbegin()->second);
            node->width = std::fabs(*right->x - *left->x);
            node->height = std::fabs(*bottom->y - *top->y);
            node->x = *left->x + node->width / 2;
            node->y = *top->y + node->height / 2;
        }
        for (const std::string& v : g_.nodes())
            if (g_.node(v)->dummy == Dummy::Border) g_.remove_node(v);
    }

    void fixup_edge_label_coords() {
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (!edge->x) continue;
            if (edge->labelpos == LabelPos::Left || edge->labelpos == LabelPos::Right) edge->width -= edge->labeloffset;
            if (edge->labelpos == LabelPos::Left) *edge->x -= edge->width / 2 + edge->labeloffset;
            if (edge->labelpos == LabelPos::Right) *edge->x += edge->width / 2 + edge->labeloffset;
        }
    }

    void translate_graph() {
        double min_x = std::numeric_limits<double>::infinity();
        double max_x = 0;
        double min_y = std::numeric_limits<double>::infinity();
        double max_y = 0;
        GraphLabel& graph = g_.graph();
        const double margin_x = graph.marginx;
        const double margin_y = graph.marginy;

        auto extremes = [&](double x, double y, double w, double h) {
            min_x = MERMAID_MIN(min_x, x - w / 2);
            max_x = MERMAID_MAX(max_x, x + w / 2);
            min_y = MERMAID_MIN(min_y, y - h / 2);
            max_y = MERMAID_MAX(max_y, y + h / 2);
        };
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr n = g_.node(v);
            extremes(*n->x, *n->y, n->width, n->height);
        }
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (edge->x) extremes(*edge->x, *edge->y, edge->width, edge->height);
        }

        min_x -= margin_x;
        min_y -= margin_y;
        for (const std::string& v : g_.nodes()) {
            const NodeLabelPtr n = g_.node(v);
            *n->x -= min_x;
            *n->y -= min_y;
        }
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (edge->points)
                for (Point& p : *edge->points) {
                    p.x -= min_x;
                    p.y -= min_y;
                }
            if (edge->x) *edge->x -= min_x;
            if (edge->y) *edge->y -= min_y;
        }
        graph.width = max_x - min_x + margin_x;
        graph.height = max_y - min_y + margin_y;
    }

    void assign_node_intersects() {
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            const NodeLabelPtr node_v = g_.node(e.v);
            const NodeLabelPtr node_w = g_.node(e.w);
            Point p1;
            Point p2;
            if (!edge->points) {
                edge->points = std::vector<Point>();
                p1 = {*node_w->x, *node_w->y};
                p2 = {*node_v->x, *node_v->y};
            } else {
                p1 = edge->points->front();
                p2 = edge->points->back();
            }
            edge->points->insert(edge->points->begin(), intersect_rect(*node_v, p1));
            edge->points->push_back(intersect_rect(*node_w, p2));
        }
    }

    void reverse_points_for_reversed_edges() {
        for (const EdgeObj& e : g_.edges()) {
            const EdgeLabelPtr edge = g_.edge(e);
            if (edge->reversed) std::reverse(edge->points->begin(), edge->points->end());
        }
    }

    // ---- layout graph -> result

    Result collect_result() const {
        Result result;
        result.width = g_.graph().width;
        result.height = g_.graph().height;
        for (const Node& n : input_.nodes) {
            const NodeLabelPtr label = g_.node(n.id);
            NodeResult r;
            r.id = n.id;
            r.x = *label->x;
            r.y = *label->y;
            const bool compound = g_.has_children(n.id);
            r.width = compound ? label->width : n.width;
            r.height = compound ? label->height : n.height;
            result.nodes.push_back(r);
        }
        for (const Edge& e : input_.edges) {
            const EdgeLabelPtr label = g_.edge(e.v, e.w, e.name);
            EdgeResult r;
            r.v = e.v;
            r.w = e.w;
            r.name = e.name;
            if (label->points) r.points = *label->points;
            r.has_label = label->x.has_value();
            if (r.has_label) r.label = {*label->x, *label->y};
            r.label_width = e.width;
            r.label_height = e.height;
            result.edges.push_back(std::move(r));
        }
        return result;
    }

    const Input& input_;
    LayoutGraph g_;
    IdGenerator ids_;
};

}  // namespace

Result layout(const Input& input) { return LayoutPipeline(input).run(); }

}  // namespace mermaid::dagre

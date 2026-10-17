#include <mermaid/flowchart_layout.h>

#include <algorithm>
#include <map>
#include <optional>

#include "layered_graph.h"
#include "layout_steps.h"
#include "node_sizer.h"

namespace mermaid::flowchart {

namespace {

using detail::Chain;
using detail::LayeredGraph;
using detail::Vertex;
using detail::VertexKind;

constexpr double kLoopWidth = 20;

// Who contains whom: subgraph nesting and the subgraph path of every node.
class Topology {
public:
    explicit Topology(const Model& model) : model_(model) {
        for (size_t i = 0; i < model.nodes.size(); ++i) node_index_[model.nodes[i].id] = static_cast<int>(i);
        for (size_t i = 0; i < model.subgraphs.size(); ++i) sub_index_[model.subgraphs[i].id] = static_cast<int>(i);

        sub_parent_.assign(model.subgraphs.size(), -1);
        for (size_t s = 0; s < model.subgraphs.size(); ++s)
            for (const std::string& member : model.subgraphs[s].members) {
                const auto it = sub_index_.find(member);
                if (it != sub_index_.end() && it->second != static_cast<int>(s) &&
                    sub_parent_[static_cast<size_t>(it->second)] < 0)
                    sub_parent_[static_cast<size_t>(it->second)] = static_cast<int>(s);
            }

        // A node belongs to the deepest subgraph that lists it.
        node_parent_.assign(model.nodes.size(), -1);
        for (size_t s = 0; s < model.subgraphs.size(); ++s)
            for (const std::string& member : model.subgraphs[s].members) {
                const auto it = node_index_.find(member);
                if (it == node_index_.end()) continue;
                int& current = node_parent_[static_cast<size_t>(it->second)];
                if (current < 0 || depth(static_cast<int>(s)) > depth(current)) current = static_cast<int>(s);
            }
    }

    int node(const std::string& id) const {
        const auto it = node_index_.find(id);
        return it == node_index_.end() ? -1 : it->second;
    }

    // A node, or the first node found inside the subgraph with this id.
    int leaf_of(const std::string& id, int guard = 0) const {
        if (const int n = node(id); n >= 0) return n;
        const auto it = sub_index_.find(id);
        if (it == sub_index_.end() || guard > 32) return -1;
        for (const std::string& member : model_.subgraphs[static_cast<size_t>(it->second)].members)
            if (const int n = leaf_of(member, guard + 1); n >= 0) return n;
        return -1;
    }

    std::vector<int> path_of(int node) const {  // outermost first
        std::vector<int> path;
        for (int s = node_parent_[static_cast<size_t>(node)]; s >= 0; s = sub_parent_[static_cast<size_t>(s)])
            path.push_back(s);
        std::reverse(path.begin(), path.end());
        return path;
    }

    const std::vector<int>& sub_parents() const { return sub_parent_; }
    int sub(const std::string& id) const {
        const auto it = sub_index_.find(id);
        return it == sub_index_.end() ? -1 : it->second;
    }

private:
    int depth(int s) const {
        int d = 0;
        for (; sub_parent_[static_cast<size_t>(s)] >= 0 && d < 64; s = sub_parent_[static_cast<size_t>(s)]) ++d;
        return d;
    }

    const Model& model_;
    std::map<std::string, int> node_index_, sub_index_;
    std::vector<int> sub_parent_, node_parent_;
};

struct EdgeInfo {
    int from = -1;
    int to = -1;
    bool placed = false;    // takes part in the layered layout
    bool loop = false;
    bool reversed = false;
    bool labelled = false;
    Size label;             // real (not flow) orientation
};

std::vector<int> common_prefix(const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> out;
    for (size_t i = 0; i < a.size() && i < b.size() && a[i] == b[i]; ++i) out.push_back(a[i]);
    return out;
}

class LayoutBuilder {
public:
    LayoutBuilder(const Model& model, const TextMeasurer& measurer, const LayoutOptions& options)
        : model_(model),
          measurer_(measurer),
          options_(options),
          topology_(model),
          flipped_(model.direction == Direction::LR || model.direction == Direction::RL) {}

    Layout build() {
        measure_nodes();
        resolve_edges();
        rank_nodes();
        build_graph();
        detail::CrossingMinimizer().run(graph_);
        assign_coordinates();
        return finish(detail::EdgeRouter(options_.rank_sep).route(graph_));
    }

private:
    // ---- preparation
    void measure_nodes() {
        const detail::NodeSizer sizer(measurer_, options_);
        for (const Node& n : model_.nodes) sizes_.push_back(sizer.size(n));
    }

    void resolve_edges() {
        for (const Edge& e : model_.edges) {
            EdgeInfo info;
            info.from = topology_.leaf_of(e.from);
            info.to = topology_.leaf_of(e.to);
            info.loop = info.from >= 0 && info.from == info.to;
            info.placed = info.from >= 0 && info.to >= 0 && !info.loop;
            info.labelled = !e.label.empty();
            if (info.labelled) info.label = measurer_.measure(e.label);
            edges_.push_back(info);
        }
    }

    void rank_nodes() {
        std::vector<size_t> placed;
        std::vector<std::pair<int, int>> pairs;
        for (size_t i = 0; i < edges_.size(); ++i)
            if (edges_[i].placed) {
                placed.push_back(i);
                pairs.emplace_back(edges_[i].from, edges_[i].to);
            }
        const std::vector<bool> flip = detail::CycleBreaker().reversed(static_cast<int>(model_.nodes.size()), pairs);

        std::vector<detail::RankEdge> rank_edges;
        for (size_t k = 0; k < placed.size(); ++k) {
            EdgeInfo& e = edges_[placed[k]];
            e.reversed = flip[k];
            const int length = std::max(1, model_.edges[placed[k]].length);
            rank_edges.push_back({e.reversed ? e.to : e.from, e.reversed ? e.from : e.to, length});
        }
        rank_ = detail::Ranker().rank(static_cast<int>(model_.nodes.size()), rank_edges);
    }

    Size flow(const Size& s) const { return flipped_ ? Size{s.height, s.width} : s; }

    void build_graph() {
        for (size_t i = 0; i < model_.nodes.size(); ++i) {
            Vertex v;
            v.node = static_cast<int>(i);
            v.size = flow(sizes_[i]);
            v.layer = 2 * rank_[i];
            v.clusters = topology_.path_of(static_cast<int>(i));
            graph_.vertices.push_back(v);
        }
        for (const EdgeInfo& e : edges_)
            if (e.loop) graph_.vertices[static_cast<size_t>(e.from)].right_room = kLoopWidth + 10;

        for (size_t i = 0; i < edges_.size(); ++i)
            if (edges_[i].placed) add_chain(static_cast<int>(i));

        graph_.build_layers();
        graph_.build_adjacency();
    }

    void add_chain(int edge) {
        const EdgeInfo& e = edges_[static_cast<size_t>(edge)];
        const int lo = e.reversed ? e.to : e.from;
        const int hi = e.reversed ? e.from : e.to;
        const int first = graph_.vertices[static_cast<size_t>(lo)].layer;
        const int last = graph_.vertices[static_cast<size_t>(hi)].layer;
        const int span = (last - first) / 2;
        const int label_layer = first + 2 * (span / 2) + 1;  // a corridor between node layers
        const std::vector<int> clusters = common_prefix(graph_.vertices[static_cast<size_t>(lo)].clusters,
                                                        graph_.vertices[static_cast<size_t>(hi)].clusters);

        Chain chain;
        chain.edge = edge;
        chain.reversed = e.reversed;
        chain.vertices.push_back(lo);
        for (int layer = first + 1; layer < last; ++layer) {
            Vertex v;
            v.edge = edge;
            v.layer = layer;
            v.clusters = clusters;
            if (e.labelled && layer == label_layer) {
                v.kind = VertexKind::Label;
                const Size s = flow(e.label);
                v.size = {s.width, s.height + options_.rank_sep};  // the band keeps its own distance
            } else {
                v.kind = VertexKind::Waypoint;
            }
            chain.vertices.push_back(static_cast<int>(graph_.vertices.size()));
            graph_.vertices.push_back(v);
        }
        chain.vertices.push_back(hi);
        graph_.chains.push_back(std::move(chain));
    }

    // ---- coordinates
    std::vector<int> cluster_leaves(int sub) const {
        std::vector<int> leaves;
        for (size_t n = 0; n < model_.nodes.size(); ++n) {
            const std::vector<int> path = topology_.path_of(static_cast<int>(n));
            if (std::find(path.begin(), path.end(), sub) != path.end()) leaves.push_back(static_cast<int>(n));
        }
        return leaves;
    }

    double title_height(const Subgraph& s) const {
        return s.title.empty() ? 0.0 : measurer_.measure(s.title).height;
    }

    void assign_coordinates() {
        detail::Spacing spacing;
        spacing.node_sep = options_.node_sep;
        spacing.edge_sep = options_.edge_sep;
        spacing.rank_sep = options_.rank_sep;
        spacing.cluster_pad = options_.cluster_pad;
        spacing.extra_gap.assign(graph_.layers.size() > 1 ? graph_.layers.size() - 1 : 0, 0.0);

        const Direction dir = model_.direction;
        for (size_t s = 0; s < model_.subgraphs.size(); ++s) {
            const std::vector<int> leaves = cluster_leaves(static_cast<int>(s));
            if (leaves.empty()) continue;
            int first = graph_.vertices[static_cast<size_t>(leaves[0])].layer, last = first;
            for (int n : leaves) {
                first = std::min(first, graph_.vertices[static_cast<size_t>(n)].layer);
                last = std::max(last, graph_.vertices[static_cast<size_t>(n)].layer);
            }
            const double title = title_height(model_.subgraphs[s]);
            const double before = options_.cluster_pad + (dir == Direction::TB ? title : 0);
            const double after = options_.cluster_pad + (dir == Direction::BT ? title : 0);
            if (first > 0) spacing.extra_gap[static_cast<size_t>(first - 1)] += before;
            if (static_cast<size_t>(last) + 1 < graph_.layers.size()) spacing.extra_gap[static_cast<size_t>(last)] += after;
        }
        detail::CoordinateAssigner(spacing).assign(graph_);
    }

    // ---- result in the real direction
    Point to_real(double x, double y) const {
        switch (model_.direction) {
            case Direction::TB: return {x, y};
            case Direction::BT: return {x, -y};
            case Direction::LR: return {y, x};
            case Direction::RL: return {-y, x};
        }
        return {x, y};
    }

    Layout finish(const std::vector<std::vector<Point>>& routes) {
        Layout layout;
        for (size_t i = 0; i < model_.nodes.size(); ++i) {
            const Vertex& v = graph_.vertices[i];
            layout.nodes.push_back({model_.nodes[i].id, model_.nodes[i].shape, to_real(v.x, v.y), sizes_[i]});
        }

        for (size_t i = 0; i < model_.edges.size(); ++i) layout.edges.push_back({model_.edges[i].id, {}, false, {}});
        for (size_t c = 0; c < graph_.chains.size(); ++c) {
            EdgeLayout& out = layout.edges[static_cast<size_t>(graph_.chains[c].edge)];
            for (const Point& p : routes[c]) out.points.push_back(to_real(p.x, p.y));
            for (int v : graph_.chains[c].vertices) {
                const Vertex& vx = graph_.vertices[static_cast<size_t>(v)];
                if (vx.kind != VertexKind::Label) continue;
                out.has_label = true;
                out.label = Rect::from_center(to_real(vx.x, vx.y), edges_[static_cast<size_t>(vx.edge)].label);
            }
        }
        for (size_t i = 0; i < edges_.size(); ++i)
            if (edges_[i].loop) layout.edges[i].points = loop_points(layout.nodes[static_cast<size_t>(edges_[i].from)]);

        add_clusters(layout);
        fit_to_margin(layout);
        return layout;
    }

    std::vector<Point> loop_points(const NodeLayout& n) const {
        const Rect r = n.rect();
        const double dy = r.height / 4;
        const double side = model_.direction == Direction::RL ? r.x : r.right();
        const double out = model_.direction == Direction::RL ? -kLoopWidth : kLoopWidth;
        const double cy = n.center.y;
        return {{side, cy - dy}, {side + out, cy - dy}, {side + out, cy + dy}, {side, cy + dy}};
    }

    std::optional<Rect> add_cluster(int sub, const Layout& layout, std::vector<ClusterLayout>& out) const {
        const Subgraph& s = model_.subgraphs[static_cast<size_t>(sub)];
        std::optional<Rect> box;
        auto include = [&](const Rect& r) { box = box ? box->united(r) : r; };
        for (const std::string& member : s.members) {
            if (const int child = topology_.sub(member); child >= 0 && child != sub) {
                if (topology_.sub_parents()[static_cast<size_t>(child)] == sub)
                    if (auto r = add_cluster(child, layout, out)) include(*r);
            } else if (const int n = topology_.node(member); n >= 0) {
                include(layout.nodes[static_cast<size_t>(n)].rect());
            }
        }
        if (!box) return std::nullopt;

        Rect r = box->expanded(options_.cluster_pad);
        const double title = title_height(s);
        r.y -= title;
        r.height += title;
        const double need = s.title.empty() ? 0.0 : measurer_.measure(s.title).width + 2 * options_.cluster_pad;
        if (need > r.width) {
            r.x -= (need - r.width) / 2;
            r.width = need;
        }
        out.push_back({s.id, s.title, r});
        return r;
    }

    void add_clusters(Layout& layout) const {
        for (size_t s = 0; s < model_.subgraphs.size(); ++s)
            if (topology_.sub_parents()[s] < 0) add_cluster(static_cast<int>(s), layout, layout.clusters);
    }

    void fit_to_margin(Layout& layout) const {
        std::optional<Rect> bounds;
        auto include = [&](const Rect& r) { bounds = bounds ? bounds->united(r) : r; };
        for (const NodeLayout& n : layout.nodes) include(n.rect());
        for (const ClusterLayout& c : layout.clusters) include(c.rect);
        for (const EdgeLayout& e : layout.edges) {
            if (e.has_label) include(e.label);
            for (const Point& p : e.points) include({p.x, p.y, 0, 0});
        }
        if (!bounds) return;

        const double dx = options_.margin - bounds->x, dy = options_.margin - bounds->y;
        for (NodeLayout& n : layout.nodes) n.center = {n.center.x + dx, n.center.y + dy};
        for (ClusterLayout& c : layout.clusters) c.rect = {c.rect.x + dx, c.rect.y + dy, c.rect.width, c.rect.height};
        for (EdgeLayout& e : layout.edges) {
            if (e.has_label) e.label = {e.label.x + dx, e.label.y + dy, e.label.width, e.label.height};
            for (Point& p : e.points) p = {p.x + dx, p.y + dy};
        }
        layout.bounds = {options_.margin, options_.margin, bounds->width, bounds->height};
        layout.size = {layout.bounds.right() + options_.margin, layout.bounds.bottom() + options_.margin};
    }

    const Model& model_;
    const TextMeasurer& measurer_;
    const LayoutOptions& options_;
    Topology topology_;
    bool flipped_;
    std::vector<Size> sizes_;
    std::vector<EdgeInfo> edges_;
    std::vector<int> rank_;
    LayeredGraph graph_;
};

}  // namespace

const NodeLayout* Layout::find_node(const std::string& id) const {
    for (const NodeLayout& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

Layout LayoutEngine::layout(const Model& model) const {
    // The parser keeps a node for a subgraph id used as an edge end (as merman
    // does); it is the subgraph that gets drawn, not that node.
    Model drawn = model;
    drawn.nodes.erase(std::remove_if(drawn.nodes.begin(), drawn.nodes.end(),
                                     [&](const Node& n) { return model.find_subgraph(n.id) != nullptr; }),
                      drawn.nodes.end());
    return LayoutBuilder(drawn, measurer_, options_).build();
}

}  // namespace mermaid::flowchart

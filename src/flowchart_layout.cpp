#include <mermaid/flowchart_layout.h>
#include <mermaid/edge_geometry.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

#include <mermaid/dagre.h>

#include "markdown_label.h"
#include "node_sizer.h"
#include <mermaid/macros.h>

namespace mermaid::flowchart {

namespace {

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

// What an edge end resolves to: a node, or a subgraph that is itself a node of its graph (a closed cluster).
struct EndRef {
    enum class kind { none, node, sub } type = kind::none;
    int index = -1;
};

struct EdgeInfo {
    EndRef from, to;
    int scope = -1;  // the closed subgraph whose graph holds this edge, -1 for the root graph
    Size label;
    bool label_wrapped = false;
    bool placed() const { return from.type != EndRef::kind::none && to.type != EndRef::kind::none; }
};

dagre::RankDir rank_dir(Direction d) {
    switch (d) {
        case Direction::TB: return dagre::RankDir::TB;
        case Direction::BT: return dagre::RankDir::BT;
        case Direction::LR: return dagre::RankDir::LR;
        case Direction::RL: return dagre::RankDir::RL;
    }
    return dagre::RankDir::TB;
}

// One dagre graph: the root graph, or the inside of a closed subgraph.
struct Scope {
    int owner = -1;  // the closed subgraph drawn by this graph, -1 for the root
    dagre::RankDir rankdir = dagre::RankDir::TB;
    double nodesep = 50;
    double ranksep = 50;
    std::vector<int> leaves;     // model nodes laid out here
    std::vector<int> compounds;  // subgraphs that stay compound nodes here
    std::vector<int> closed;     // subgraphs that are laid out on their own and are nodes here
};

class LayoutBuilder {
public:
    LayoutBuilder(const Model& model, const TextMeasurer& measurer, const LayoutOptions& options)
        : model_(model), measurer_(measurer), options_(options), topology_(model) {}

    Layout build() {
        const detail::NodeSizer sizer(measurer_, options_, model_.direction);
        for (const Node& n : model_.nodes) {
            sizes_.push_back(sizer.size(n));
            const detail::NodeSizer::Label label = sizer.label(n);
            labels_.push_back(label.size);
            labels_wrapped_.push_back(label.wrapped);
        }
        compute_used();
        resolve_edges();
        number_clusters();

        Scope root;
        root.rankdir = rank_dir(model_.direction);
        root.nodesep = options_.node_sep;
        root.ranksep = options_.rank_sep;
        collect(-1, root);
        return layout_scope(root);
    }

private:
    using Result = dagre::Result;

    size_t count() const { return model_.subgraphs.size(); }

    bool has_nodes(int sub) const {
        for (size_t n = 0; n < model_.nodes.size(); ++n) {
            const std::vector<int> path = topology_.path_of(static_cast<int>(n));
            if (std::find(path.begin(), path.end(), sub) != path.end()) return true;
        }
        return false;
    }

    void compute_used() {
        used_.assign(count(), false);
        for (size_t s = 0; s < count(); ++s) used_[s] = has_nodes(static_cast<int>(s));  // closed upwards
    }

    // ---- which subgraphs are closed (mermaid-graphlib.js `adjustClustersAndEdges` + `extractor`)

    // Strict descendant, as in `isDescendant`: a node or subgraph id lies below `cluster`.
    bool below(const std::string& id, int cluster) const {
        if (const int n = topology_.node(id); n >= 0) {
            const std::vector<int> path = topology_.path_of(n);
            return std::find(path.begin(), path.end(), cluster) != path.end();
        }
        int s = topology_.sub(id);
        if (s < 0) return false;
        for (s = topology_.sub_parents()[static_cast<size_t>(s)]; s >= 0; s = topology_.sub_parents()[static_cast<size_t>(s)])
            if (s == cluster) return true;
        return false;
    }

    int parent_of(const EndRef& r) const {
        if (r.type == EndRef::kind::node) {
            const std::vector<int> path = topology_.path_of(r.index);
            return path.empty() ? -1 : path.back();
        }
        return topology_.sub_parents()[static_cast<size_t>(r.index)];
    }

    void resolve_edges() {
        // An edge with exactly one end below a cluster leaves it.
        external_.assign(count(), false);
        for (size_t c = 0; c < count(); ++c) {
            if (!used_[c]) continue;
            for (const Edge& e : model_.edges)
                if (below(e.from, static_cast<int>(c)) != below(e.to, static_cast<int>(c))) external_[c] = true;
        }

        // The node standing in for a cluster that has an edge leaving it (the first leaf, or the nested
        // cluster holding it when that one stays closed).
        anchor_.assign(count(), EndRef{});
        for (size_t c = 0; c < count(); ++c) {
            if (!used_[c]) continue;
            const int leaf = topology_.leaf_of(model_.subgraphs[c].id);
            if (leaf < 0) continue;
            EndRef anchor{EndRef::kind::node, leaf};
            const int parent = parent_of(anchor);
            if (parent >= 0 && parent != static_cast<int>(c) && used_[static_cast<size_t>(parent)] &&
                !external_[static_cast<size_t>(parent)])
                anchor = {EndRef::kind::sub, parent};
            anchor_[c] = anchor;
        }

        auto resolve = [&](const std::string& id) -> EndRef {
            if (const int n = topology_.node(id); n >= 0) return {EndRef::kind::node, n};
            const int s = topology_.sub(id);
            if (s < 0 || !used_[static_cast<size_t>(s)]) return {};
            if (!external_[static_cast<size_t>(s)]) return {EndRef::kind::sub, s};
            const EndRef anchor = anchor_[static_cast<size_t>(s)];
            // the anchor's parent now has an edge reaching into it
            if (const int parent = parent_of(anchor); parent >= 0) external_[static_cast<size_t>(parent)] = true;
            return anchor;
        };
        edges_.clear();
        for (const Edge& e : model_.edges) {
            EdgeInfo info;
            info.from = resolve(e.from);
            info.to = resolve(e.to);
            if (!e.label.empty()) {
                const TextMeasurer::Wrapped label =
                    e.markdown_label ? mermaid::detail::markdown_label(e.label).measure(measurer_, options_.edge_label_max_width)
                                     : measurer_.measure_wrapped(e.label, options_.edge_label_max_width);
                info.label = label.size;
                info.label_wrapped = label.wrapped;
            }
            edges_.push_back(info);
        }

        closed_.assign(count(), false);
        for (size_t c = 0; c < count(); ++c) closed_[c] = used_[c] && !external_[c];
        for (EdgeInfo& e : edges_) e.scope = edge_scope(e);
    }

    // The closed subgraphs a position lies in, outermost first. A subgraph end is not inside itself.
    std::vector<int> closed_chain(const EndRef& r) const {
        std::vector<int> chain;
        for (int s = parent_of(r); s >= 0; s = topology_.sub_parents()[static_cast<size_t>(s)])
            if (closed_[static_cast<size_t>(s)]) chain.push_back(s);
        std::reverse(chain.begin(), chain.end());
        return chain;
    }

    int edge_scope(const EdgeInfo& e) const {
        if (!e.placed()) return -1;
        const std::vector<int> a = closed_chain(e.from), b = closed_chain(e.to);
        int scope = -1;
        for (size_t i = 0; i < a.size() && i < b.size() && a[i] == b[i]; ++i) scope = a[i];
        return scope;
    }

    // ---- the members of a graph

    void collect(int container, Scope& scope) const {
        const size_t first_leaf = scope.leaves.size();
        collect_members(container, scope);
        std::sort(scope.leaves.begin() + static_cast<std::ptrdiff_t>(first_leaf), scope.leaves.end());  // model order
    }

    void collect_members(int container, Scope& scope) const {
        for (size_t n = 0; n < model_.nodes.size(); ++n) {
            const std::vector<int> path = topology_.path_of(static_cast<int>(n));
            if ((path.empty() ? -1 : path.back()) == container) scope.leaves.push_back(static_cast<int>(n));
        }
        // dagre sees the subgraphs in reverse declaration order (flowDb.getData)
        for (size_t i = count(); i-- > 0;) {
            if (!used_[i] || topology_.sub_parents()[i] != container) continue;
            if (closed_[i]) {
                scope.closed.push_back(static_cast<int>(i));
            } else {
                scope.compounds.push_back(static_cast<int>(i));
                collect_members(static_cast<int>(i), scope);
            }
        }
    }

    // ---- laying out one graph

    static dagre::RankDir perpendicular(dagre::RankDir d) {
        return d == dagre::RankDir::TB ? dagre::RankDir::LR : dagre::RankDir::TB;
    }

    const std::string& sub_id(int s) const { return model_.subgraphs[static_cast<size_t>(s)].id; }

    std::string end_id(const EndRef& r) const {
        return r.type == EndRef::kind::node ? model_.nodes[static_cast<size_t>(r.index)].id : sub_id(r.index);
    }

    Layout layout_scope(const Scope& scope) {
        // The inside of every closed subgraph first: its box is the node the parent sees.
        std::vector<RootLayout> roots;
        std::vector<Size> closed_size;
        for (const int c : scope.closed) {
            Scope inner;
            inner.owner = c;
            const std::optional<Direction>& explicit_dir = model_.subgraphs[static_cast<size_t>(c)].direction;
            inner.rankdir = explicit_dir ? rank_dir(*explicit_dir) : perpendicular(scope.rankdir);
            inner.nodesep = scope.nodesep;
            inner.ranksep = scope.ranksep + 25;
            collect(c, inner);
            RootLayout root;
            root.id = sub_id(c);
            root.layout = std::make_shared<const Layout>(layout_scope(inner));
            const Rect& box = root.layout->clusters.back().rect;  // the owner is the last cluster
            closed_size.push_back({box.width, box.height});
            roots.push_back(std::move(root));
        }

        const Result result = dagre::layout(make_input(scope, closed_size));
        return finish(scope, result, std::move(roots), closed_size);
    }

    dagre::Input make_input(const Scope& scope, const std::vector<Size>& closed_size) const {
        dagre::Input in;
        in.options.rankdir = scope.rankdir;
        in.options.nodesep = scope.nodesep;
        in.options.ranksep = scope.ranksep;
        in.options.edgesep = options_.edge_sep;
        in.options.marginx = options_.margin;  // prepareLayoutForDagre: marginx / marginy 8
        in.options.marginy = options_.margin;

        auto parent_id = [&](int sub) -> std::string { return sub >= 0 ? sub_id(sub) : std::string(); };
        // compounds and closed subgraphs share the subgraph order of dagre's node list
        std::vector<int> subs = scope.compounds;
        subs.insert(subs.end(), scope.closed.begin(), scope.closed.end());
        std::sort(subs.begin(), subs.end(), [](int a, int b) { return a > b; });
        for (const int s : subs) {
            const auto closed = std::find(scope.closed.begin(), scope.closed.end(), s);
            const std::string parent = parent_id(topology_.sub_parents()[static_cast<size_t>(s)]);
            if (closed == scope.closed.end()) {
                in.nodes.push_back({sub_id(s), 0, 0, parent});
            } else {
                const Size& size = closed_size[static_cast<size_t>(closed - scope.closed.begin())];
                in.nodes.push_back({sub_id(s), size.width, size.height, parent});
            }
        }
        for (const int i : scope.leaves) {
            const std::vector<int> path = topology_.path_of(i);
            in.nodes.push_back({model_.nodes[static_cast<size_t>(i)].id, sizes_[static_cast<size_t>(i)].width,
                                sizes_[static_cast<size_t>(i)].height, parent_id(path.empty() ? -1 : path.back())});
        }
        // prepareLayoutForDagre: a self-loop gets two helper nodes (children of the same parent) ...
        for (size_t i = 0; i < edges_.size(); ++i) {
            if (!is_self_loop(i) || edges_[i].scope != scope.owner || first_loop(i) != i) continue;
            const std::string parent = parent_id(parent_of(edges_[i].from));
            in.nodes.push_back({loop_helper_id(i, 1), kLoopHelperSize, kLoopHelperSize, parent});
            in.nodes.push_back({loop_helper_id(i, 2), kLoopHelperSize, kLoopHelperSize, parent});
        }
        if (scope.owner >= 0) in.nodes.push_back({sub_id(scope.owner), 0, 0, std::string()});

        for (size_t i = 0; i < edges_.size(); ++i) {
            if (!edges_[i].placed() || edges_[i].scope != scope.owner) continue;
            if (is_self_loop(i)) {  // ... and three edges: node -> helper 1 -> helper 2 -> node; the label is on the middle one
                // The keys repeat for a second loop on the node: it replaces the first one's data in the first one's place.
                if (first_loop(i) != i) continue;
                const size_t i_last = last_loop(i);
                const std::string node = end_id(edges_[i].from);
                for (int part = 0; part < 3; ++part) {
                    dagre::Edge e;
                    e.v = part == 0 ? node : loop_helper_id(i, part);
                    e.w = part == 2 ? node : loop_helper_id(i, part + 1);
                    e.name = node + "-cyclic-special-" + std::to_string(part);
                    e.minlen = MERMAID_MAX(1, model_.edges[i_last].length);
                    e.labelpos = dagre::LabelPos::Center;
                    if (part == 1 && (edges_[i_last].label.width > 0 || edges_[i_last].label.height > 0)) {
                        e.width = edges_[i_last].label.width + 2 * options_.edge_label_pad;
                        e.height = edges_[i_last].label.height + 2 * options_.edge_label_pad;
                    }
                    in.edges.push_back(e);
                }
                continue;
            }
            dagre::Edge e;
            e.v = end_id(edges_[i].from);
            e.w = end_id(edges_[i].to);
            e.name = std::to_string(i);
            e.minlen = MERMAID_MAX(1, model_.edges[i].length);
            e.labelpos = dagre::LabelPos::Center;  // Mermaid sets labelpos 'c' on every edge
            if (edges_[i].label.width > 0 || edges_[i].label.height > 0) {
                e.width = edges_[i].label.width + 2 * options_.edge_label_pad;
                e.height = edges_[i].label.height + 2 * options_.edge_label_pad;
            }
            in.edges.push_back(e);
        }
        return in;
    }

    // The helpers are given 10 x 10, but Mermaid measures them once they are drawn (an empty rect of 0.1 x 0.1) and
    // dagre lays out with that size.
    static constexpr double kLoopHelperSize = 0.1;

    bool is_self_loop(size_t i) const {
        const EdgeInfo& e = edges_[i];
        // a loop on a node, or on a closed subgraph (which is one node of the parent graph)
        return e.placed() && e.from.type == e.to.type && e.from.index == e.to.index;
    }

    // Loops on the same node in the same scope share their helper nodes and edge keys (see make_input).
    bool same_loop_node(size_t a, size_t b) const {
        return is_self_loop(b) && edges_[a].scope == edges_[b].scope && edges_[a].from.type == edges_[b].from.type &&
               edges_[a].from.index == edges_[b].from.index;
    }
    size_t first_loop(size_t i) const {
        for (size_t j = 0; j < i; ++j)
            if (same_loop_node(i, j)) return j;
        return i;
    }
    size_t last_loop(size_t i) const {
        size_t last = i;
        for (size_t j = i + 1; j < edges_.size(); ++j)
            if (same_loop_node(i, j)) last = j;
        return last;
    }

    std::string loop_helper_id(size_t i, int part) const {
        const std::string node = end_id(edges_[i].from);
        return node + "---" + node + "---" + std::to_string(part);
    }

    static double clamp(double v, double lo, double hi) { return MERMAID_MAX(lo, MERMAID_MIN(hi, v)); }

    // dagre/index.js getEdgesToRender: the three segments become one four-point route on the side of the node
    // the helpers were laid out towards; the label sits beside it.
    EdgeLayout merged_self_loop(size_t i, const Scope& scope, const Result& result, EdgeLayout out) const {
        out.self_loop = true;
        out.label_wrapped = edges_[i].label_wrapped;
        // The loop is drawn around what the edge names: for an open cluster that is its compound box, although the
        // layout graph holds the loop on the node standing in for the cluster.
        const dagre::NodeResult* cluster = topology_.sub(model_.edges[i].from) >= 0 ? result.find_node(model_.edges[i].from) : nullptr;
        const dagre::NodeResult* node = cluster ? cluster : result.find_node(end_id(edges_[i].from));
        const dagre::NodeResult* h1 = result.find_node(loop_helper_id(i, 1));
        const dagre::NodeResult* h2 = result.find_node(loop_helper_id(i, 2));

        // getSelfLoopSide: towards the middle of the helper nodes
        const double dx = (h1->x + h2->x) / 2 - node->x, dy = (h1->y + h2->y) / 2 - node->y;
        enum class side { top, bottom, left, right } where;
        if (std::fabs(dx) > std::fabs(dy)) where = dx > 0 ? side::right : side::left;
        else if (std::fabs(dy) > 0) where = dy > 0 ? side::bottom : side::top;
        else
            where = scope.rankdir == dagre::RankDir::BT ? side::bottom
                    : scope.rankdir == dagre::RankDir::LR ? side::right
                    : scope.rankdir == dagre::RankDir::RL ? side::left
                                                           : side::top;

        // getSelfLoopPoints
        const double label_w = edges_[i].label.width > 0 || edges_[i].label.height > 0 ? edges_[i].label.width + 2 * options_.edge_label_pad : 0;
        const double label_h = edges_[i].label.width > 0 || edges_[i].label.height > 0 ? edges_[i].label.height + 2 * options_.edge_label_pad : 0;
        const double x = node->x, y = node->y, half_w = node->width / 2, half_h = node->height / 2;
        const double max_span = MERMAID_MAX(36.0, MERMAID_MIN(100.0, node->width * 0.8));
        const double span = clamp(MERMAID_MAX(label_w, node->width * 0.35), 36, max_span);
        const double depth = clamp(MERMAID_MIN(node->width, node->height) * 0.45, 24, 48);
        switch (where) {
            case side::bottom:
                out.points = {{x - span / 2, y + half_h}, {x - span / 2, y + half_h + depth}, {x + span / 2, y + half_h + depth}, {x + span / 2, y + half_h}};
                break;
            case side::right:
                out.points = {{x + half_w, y - span / 2}, {x + half_w + depth, y - span / 2}, {x + half_w + depth, y + span / 2}, {x + half_w, y + span / 2}};
                break;
            case side::left:
                out.points = {{x - half_w, y - span / 2}, {x - half_w - depth, y - span / 2}, {x - half_w - depth, y + span / 2}, {x - half_w, y + span / 2}};
                break;
            case side::top:
                out.points = {{x - span / 2, y - half_h}, {x - span / 2, y - half_h - depth}, {x + span / 2, y - half_h - depth}, {x + span / 2, y - half_h}};
                break;
        }

        // getSelfLoopLabelPosition
        if (edges_[i].label.width > 0 || edges_[i].label.height > 0) {
            constexpr double gap = 4;
            double lx = x, ly = y;
            double min_x = out.points[0].x, max_x = out.points[0].x, min_y = out.points[0].y, max_y = out.points[0].y;
            for (const Point& p : out.points) {
                min_x = MERMAID_MIN(min_x, p.x);
                max_x = MERMAID_MAX(max_x, p.x);
                min_y = MERMAID_MIN(min_y, p.y);
                max_y = MERMAID_MAX(max_y, p.y);
            }
            switch (where) {
                case side::bottom: ly = max_y + label_h / 2 + gap; break;
                case side::right: lx = max_x + label_w / 2 + gap; break;
                case side::left: lx = min_x - label_w / 2 - gap; break;
                case side::top: ly = min_y - label_h / 2 - gap; break;
            }
            out.has_label = true;
            out.label = Rect::from_center({lx, ly}, edges_[i].label);
        }
        return out;
    }

    Layout finish(const Scope& scope, const Result& result, std::vector<RootLayout> roots,
                  const std::vector<Size>& closed_size) {
        Layout layout;
        for (const int i : scope.leaves) {
            const Node& node = model_.nodes[static_cast<size_t>(i)];
            const dagre::NodeResult* r = result.find_node(node.id);
            layout.nodes.push_back({node.id, node.shape, {r->x, r->y}, sizes_[static_cast<size_t>(i)],
                                    labels_[static_cast<size_t>(i)], static_cast<bool>(labels_wrapped_[static_cast<size_t>(i)])});
        }
        for (size_t i = 0; i < edges_.size(); ++i) {
            if (edges_[i].scope != scope.owner) continue;
            EdgeLayout out;
            out.id = model_.edges[i].id;
            out.model_index = i;
            out.label_wrapped = edges_[i].label_wrapped;
            if (is_self_loop(i)) {
                if (last_loop(i) != i) continue;  // replaced by a later loop on the same node
                layout.edges.push_back(merged_self_loop(i, scope, result, std::move(out)));
                for (int part = 1; part <= 2; ++part) {
                    const dagre::NodeResult* h = result.find_node(loop_helper_id(i, part));
                    layout.loop_helpers.push_back({loop_helper_id(i, part), {h->x, h->y}});
                }
                continue;
            }
            if (edges_[i].placed()) {
                const dagre::EdgeResult* r =
                    result.find_edge(end_id(edges_[i].from), end_id(edges_[i].to), std::to_string(i));
                out.points = r->points;
                if (r->has_label) {
                    out.has_label = true;
                    out.label = Rect::from_center(r->label, edges_[i].label);
                }
            }
            layout.edges.push_back(std::move(out));
        }
        for (size_t k = 0; k < roots.size(); ++k) {
            const dagre::NodeResult* r = result.find_node(roots[k].id);
            roots[k].box = Rect::from_center({r->x, r->y}, closed_size[k]);
        }
        layout.roots = std::move(roots);
        add_clusters(scope, layout, result);
        measure_bounds(layout);
        return layout;
    }

    // Mermaid hands every subgraph a slot in declaration order, depth first (flowDb.getData).
    void number_clusters() {
        color_index_.assign(count(), 0);
        int next = 0;
        for (size_t s = 0; s < count(); ++s)
            if (topology_.sub_parents()[s] < 0) number_cluster(static_cast<int>(s), next);
    }

    void number_cluster(int sub, int& next) {
        color_index_[static_cast<size_t>(sub)] = next++;
        for (size_t c = 0; c < count(); ++c)
            if (topology_.sub_parents()[c] == sub) number_cluster(static_cast<int>(c), next);
    }

    // The box of a subgraph is the dagre compound node (border nodes included). Children first, so the
    // outermost cluster comes last.
    void add_cluster(int sub, const Scope& scope, const Result& result, std::vector<ClusterLayout>& out) const {
        for (size_t c = 0; c < count(); ++c)
            if (topology_.sub_parents()[c] == sub && is_compound_in(static_cast<int>(c), scope))
                add_cluster(static_cast<int>(c), scope, result, out);

        const Subgraph& s = model_.subgraphs[static_cast<size_t>(sub)];
        const dagre::NodeResult* r = result.find_node(s.id);
        if (!r) return;

        ClusterLayout c;
        c.id = s.id;
        c.title = s.title;
        c.color_index = color_index_[static_cast<size_t>(sub)];
        c.markdown_title = s.markdown_title;
        if (!s.title.empty())
            c.label = s.markdown_title ? mermaid::detail::markdown_label(s.title).measure(measurer_, 1e9).size : measurer_.measure(s.title);
        // clusters.js: the box grows to the label width plus the cluster padding.
        const double width = MERMAID_MAX(r->width, c.label.width + options_.cluster_pad);
        c.rect = {r->x - width / 2, r->y - r->height / 2, width, r->height};
        out.push_back(std::move(c));
    }

    static bool is_compound_in(int sub, const Scope& scope) {
        return std::find(scope.compounds.begin(), scope.compounds.end(), sub) != scope.compounds.end();
    }

    void add_clusters(const Scope& scope, Layout& layout, const Result& result) const {
        if (scope.owner >= 0) {
            add_cluster(scope.owner, scope, result, layout.clusters);  // the owner comes last
            return;
        }
        for (size_t s = 0; s < count(); ++s)
            if (topology_.sub_parents()[s] < 0 && is_compound_in(static_cast<int>(s), scope))
                add_cluster(static_cast<int>(s), scope, result, layout.clusters);
    }

    // The curve of a self-loop as it is drawn (clipped to the node, corners fixed, pulled back from the arrow tips).
    Rect self_loop_bounds(const EdgeLayout& e, const Layout& layout) const {
        const Edge& edge = model_.edges[e.model_index];
        std::vector<Point> route = e.points;
        for (const NodeLayout& n : layout.nodes)
            if (n.id == edge.from) route = edge_geometry::clip_to_nodes(route, n, n);
        const std::vector<Point> drawn = edge_geometry::shorten_ends(
            edge_geometry::fix_corners(route), edge_geometry::path_offset(edge.start), edge_geometry::path_offset(edge.end));
        return edge_geometry::curve_bounds(edge.curve, drawn);
    }

    // dagre has already moved the graph by its margin (translateGraph). What is drawn can be smaller than what dagre
    // laid out (the self-loop helpers are 10 x 10 for dagre and 0.1 x 0.1 on screen), and Mermaid sizes the viewBox from
    // the drawn content, so only the bounds are measured here; nothing moves.
    void measure_bounds(Layout& layout) const {
        std::optional<Rect> bounds;
        auto include = [&](const Rect& r) { bounds = bounds ? bounds->united(r) : r; };
        for (const NodeLayout& n : layout.nodes) include(n.rect());
        for (const ClusterLayout& c : layout.clusters) include(c.rect);
        for (const RootLayout& r : layout.roots) include(r.box);
        for (const LoopHelper& h : layout.loop_helpers) include({h.center.x, h.center.y, 0.1, 0.1});  // their drawn rect
        for (const EdgeLayout& e : layout.edges) {
            if (e.has_label) include(e.label);
            if (e.self_loop) {  // sharp corners: the drawn curve is well inside its points, and getBBox sees the curve
                include(self_loop_bounds(e, layout));
                continue;
            }
            for (const Point& p : e.points) include({p.x, p.y, 0, 0});
        }
        for (RootLayout& r : layout.roots) {
            const Rect& inner = r.layout->clusters.back().rect;
            r.origin = {r.box.x - inner.x, r.box.y - inner.y};
        }
        if (!bounds) return;
        layout.bounds = *bounds;
        layout.size = {bounds->right() + options_.margin, bounds->bottom() + options_.margin};
    }

    const Model& model_;
    const TextMeasurer& measurer_;
    const LayoutOptions& options_;
    Topology topology_;
    std::vector<Size> sizes_;
    std::vector<Size> labels_;
    std::vector<char> labels_wrapped_;
    std::vector<EdgeInfo> edges_;
    std::vector<bool> used_, external_, closed_;
    std::vector<EndRef> anchor_;
    std::vector<int> color_index_;
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
    // `%%{init}%%` is part of the diagram text, so it beats the options the engine was built with.
    LayoutOptions options = options_;
    if (model.config.node_spacing) options.node_sep = *model.config.node_spacing;
    if (model.config.rank_spacing) options.rank_sep = *model.config.rank_spacing;
    return LayoutBuilder(drawn, measurer_, options).build();
}

}  // namespace mermaid::flowchart

#include <mermaid/flowchart_layout.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

#include <mermaid/dagre.h>

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

struct EdgeInfo {
    int from = -1;
    int to = -1;
    bool placed = false;  // both ends resolve to a node
    Size label;
};

// dagre routes unlabelled edges through the (empty) label rank; the extra point lies on the
// line and is dropped, as in the merman goldens.
std::vector<Point> without_collinear(const std::vector<Point>& in) {
    std::vector<Point> out;
    for (const Point& p : in) {
        while (out.size() >= 2) {
            const Point& a = out[out.size() - 2];
            const Point& b = out.back();
            const double cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
            if (std::abs(cross) > 1e-9) break;
            out.pop_back();
        }
        out.push_back(p);
    }
    return out;
}

dagre::RankDir rank_dir(Direction d) {
    switch (d) {
        case Direction::TB: return dagre::RankDir::TB;
        case Direction::BT: return dagre::RankDir::BT;
        case Direction::LR: return dagre::RankDir::LR;
        case Direction::RL: return dagre::RankDir::RL;
    }
    return dagre::RankDir::TB;
}

class LayoutBuilder {
public:
    LayoutBuilder(const Model& model, const TextMeasurer& measurer, const LayoutOptions& options)
        : model_(model), measurer_(measurer), options_(options), topology_(model) {}

    Layout build() {
        const detail::NodeSizer sizer(measurer_, options_);
        for (const Node& n : model_.nodes) sizes_.push_back(sizer.size(n));
        for (const Edge& e : model_.edges) {
            EdgeInfo info;
            info.from = topology_.leaf_of(e.from);
            info.to = topology_.leaf_of(e.to);
            info.placed = info.from >= 0 && info.to >= 0;
            if (!e.label.empty()) info.label = measurer_.measure(e.label);
            edges_.push_back(info);
        }
        return finish(dagre::layout(make_input()));
    }

private:
    bool has_nodes(int sub) const {
        for (size_t n = 0; n < model_.nodes.size(); ++n) {
            const std::vector<int> path = topology_.path_of(static_cast<int>(n));
            if (std::find(path.begin(), path.end(), sub) != path.end()) return true;
        }
        return false;
    }

    dagre::Input make_input() const {
        dagre::Input in;
        in.options.rankdir = rank_dir(model_.direction);
        in.options.nodesep = options_.node_sep;
        in.options.ranksep = options_.rank_sep;
        in.options.edgesep = options_.edge_sep;

        std::vector<bool> used(model_.subgraphs.size());
        for (size_t s = 0; s < used.size(); ++s) used[s] = has_nodes(static_cast<int>(s));
        // A parent is never empty when a child is not, so `used` is closed upwards.
        auto parent_id = [&](int sub) -> std::string {
            return sub >= 0 && used[static_cast<size_t>(sub)] ? model_.subgraphs[static_cast<size_t>(sub)].id : std::string();
        };
        for (size_t s = 0; s < model_.subgraphs.size(); ++s)
            if (used[s]) in.nodes.push_back({model_.subgraphs[s].id, 0, 0, parent_id(topology_.sub_parents()[s])});
        for (size_t i = 0; i < model_.nodes.size(); ++i) {
            const int p = topology_.path_of(static_cast<int>(i)).empty() ? -1 : topology_.path_of(static_cast<int>(i)).back();
            in.nodes.push_back({model_.nodes[i].id, sizes_[i].width, sizes_[i].height, parent_id(p)});
        }
        for (size_t i = 0; i < edges_.size(); ++i) {
            if (!edges_[i].placed) continue;
            dagre::Edge e;
            e.v = model_.nodes[static_cast<size_t>(edges_[i].from)].id;
            e.w = model_.nodes[static_cast<size_t>(edges_[i].to)].id;
            e.name = std::to_string(i);
            e.minlen = MERMAID_MAX(1, model_.edges[i].length);
            if (edges_[i].label.width > 0 || edges_[i].label.height > 0) {
                e.width = edges_[i].label.width + 2 * options_.edge_label_pad;
                e.height = edges_[i].label.height + 2 * options_.edge_label_pad;
            }
            in.edges.push_back(e);
        }
        return in;
    }

    Layout finish(const dagre::Result& result) {
        Layout layout;
        for (size_t i = 0; i < model_.nodes.size(); ++i) {
            const dagre::NodeResult* r = result.find_node(model_.nodes[i].id);
            layout.nodes.push_back({model_.nodes[i].id, model_.nodes[i].shape, {r->x, r->y}, sizes_[i]});
        }
        for (size_t i = 0; i < model_.edges.size(); ++i) {
            EdgeLayout out{model_.edges[i].id, {}, false, {}};
            if (edges_[i].placed) {
                const dagre::EdgeResult* r =
                    result.find_edge(model_.nodes[static_cast<size_t>(edges_[i].from)].id,
                                     model_.nodes[static_cast<size_t>(edges_[i].to)].id, std::to_string(i));
                out.points = without_collinear(r->points);
                if (r->has_label) {
                    out.has_label = true;
                    out.label = Rect::from_center(r->label, edges_[i].label);
                }
            }
            layout.edges.push_back(std::move(out));
        }
        add_clusters(layout);
        fit_to_margin(layout);
        return layout;
    }

    double title_height(const Subgraph& s) const {
        return s.title.empty() ? 0.0 : measurer_.measure(s.title).height;
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
    std::vector<Size> sizes_;
    std::vector<EdgeInfo> edges_;
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

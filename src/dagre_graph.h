// dagre_graph.h (private) - a directed/undirected multigraph with optional nesting.
//
// It reproduces the behaviour of graphlib (the graph library dagre is built on),
// including the order in which nodes, edges and neighbours are visited: the layout
// stages break ties by that order, so the numbers only match dagre when it matches.
// In JavaScript the keys of an object come out with integer-like keys first (ascending)
// and everything else in insertion order; `js_key_order` mirrors that for node ids.
#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <mermaid/dagre.h>

namespace mermaid::dagre::detail {

// ---------------------------------------------------------------- labels

struct EdgeObj {
    std::string v;
    std::string w;
    std::string name;  // empty = unnamed
};

enum class Dummy { None, Edge, EdgeLabel, EdgeProxy, Border, SelfEdge, Root };

// Which side of a compound node a border dummy belongs to (None for top and bottom).
enum class BorderSide { None, Left, Right };

struct EdgeLabel;

struct SelfEdgeRec {
    EdgeObj e;
    std::shared_ptr<EdgeLabel> label;
};

struct EdgeLabel {
    double minlen = 1;
    double weight = 1;
    double width = 0;
    double height = 0;
    double labeloffset = 10;
    LabelPos labelpos = LabelPos::Right;
    std::optional<std::vector<Point>> points;
    std::optional<double> x;
    std::optional<double> y;
    bool reversed = false;
    std::string forward_name;
    std::optional<double> label_rank;
    bool nesting_edge = false;
    double cutvalue = 0;  // spanning-tree edges (network simplex)
};

struct NodeLabel {
    double width = 0;
    double height = 0;
    std::optional<double> x;
    std::optional<double> y;
    std::optional<double> rank;
    std::optional<int> order;
    Dummy dummy = Dummy::None;
    std::string border_top;
    std::string border_bottom;
    std::map<int, std::string> border_left;   // by rank
    std::map<int, std::string> border_right;  // by rank
    std::optional<int> min_rank;
    std::optional<int> max_rank;
    BorderSide border_side = BorderSide::None;
    std::vector<SelfEdgeRec> self_edges;
    std::shared_ptr<EdgeLabel> edge_label;  // dummy edge nodes, self-edge nodes
    EdgeObj edge_obj;                       // dummy edge nodes, edge proxies, self-edge nodes
    std::optional<LabelPos> labelpos;       // dummy edge-label nodes
    // spanning tree (network simplex)
    int low = 0;
    int lim = 0;
    std::string parent;
    // layer graphs (ordering): the borders of a compound node in one rank
    std::string layer_border_left;
    std::string layer_border_right;
};

struct GraphLabel {
    double ranksep = 50;
    double edgesep = 20;
    double nodesep = 50;
    double marginx = 0;
    double marginy = 0;
    RankDir rankdir = RankDir::TB;
    std::vector<std::string> dummy_chains;
    std::string nesting_root;
    double node_rank_factor = 0;
    double width = 0;
    double height = 0;
    std::string root;  // layer graphs
};

// ---------------------------------------------------------------- ordering helpers

inline bool is_array_index(const std::string& s) {
    if (s.empty() || s.size() > 10) return false;
    if (s.size() > 1 && s[0] == '0') return false;
    unsigned long long value = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    return value < 4294967295ULL;
}

// Integer-like keys first (ascending), the others in insertion order.
inline std::vector<std::string> js_key_order(const std::vector<std::string>& keys) {
    std::vector<std::pair<unsigned long long, const std::string*>> ints;
    for (const std::string& k : keys)
        if (is_array_index(k)) ints.emplace_back(std::stoull(k), &k);
    if (ints.empty()) return keys;
    std::stable_sort(ints.begin(), ints.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<std::string> out;
    out.reserve(keys.size());
    for (const auto& p : ints) out.push_back(*p.second);
    for (const std::string& k : keys)
        if (!is_array_index(k)) out.push_back(k);
    return out;
}

// Insertion-ordered multiset of node ids (graphlib's `_preds` / `_sucs` / child lists).
class KeyCounts {
public:
    void increment(const std::string& k) {
        auto it = counts_.find(k);
        if (it == counts_.end()) {
            counts_[k] = 1;
            seq_.push_back(k);
        } else {
            ++it->second;
        }
    }
    void decrement(const std::string& k) {
        auto it = counts_.find(k);
        if (it == counts_.end()) return;
        if (--it->second == 0) erase(k);
    }
    void erase(const std::string& k) {
        if (counts_.erase(k)) seq_.erase(std::find(seq_.begin(), seq_.end(), k));
    }
    bool contains(const std::string& k) const { return counts_.count(k) != 0; }
    size_t size() const { return seq_.size(); }
    std::vector<std::string> keys() const { return js_key_order(seq_); }

private:
    std::vector<std::string> seq_;
    std::unordered_map<std::string, int> counts_;
};

// ---------------------------------------------------------------- the graph

template <class N, class E>
class Graph {
public:
    using NodePtr = std::shared_ptr<N>;
    using EdgePtr = std::shared_ptr<E>;
    using NodeFactory = std::function<NodePtr(const std::string&)>;

    explicit Graph(bool directed = true, bool multigraph = false, bool compound = false)
        : directed_(directed), multigraph_(multigraph), compound_(compound) {
        if (compound_) children_[root_key()];
    }

    bool is_directed() const { return directed_; }
    bool is_multigraph() const { return multigraph_; }
    bool is_compound() const { return compound_; }

    // ---- graph label
    void set_graph(std::shared_ptr<GraphLabel> label) { label_ = std::move(label); }
    GraphLabel& graph() { return *label_; }
    const GraphLabel& graph() const { return *label_; }
    std::shared_ptr<GraphLabel> graph_ptr() const { return label_; }

    void set_default_node_label(NodeFactory f) { node_factory_ = std::move(f); }

    // ---- nodes
    void set_node(const std::string& v) {
        if (nodes_.count(v)) return;
        add_node(v, node_factory_ ? node_factory_(v) : NodePtr());
    }
    void set_node(const std::string& v, NodePtr label) {
        auto it = nodes_.find(v);
        if (it != nodes_.end()) {
            it->second.label = std::move(label);
            return;
        }
        add_node(v, std::move(label));
    }
    NodePtr node(const std::string& v) const {
        const auto it = nodes_.find(v);
        return it == nodes_.end() ? NodePtr() : it->second.label;
    }
    bool has_node(const std::string& v) const { return nodes_.count(v) != 0; }
    size_t node_count() const { return nodes_.size(); }
    std::vector<std::string> nodes() const { return js_key_order(node_seq_); }

    void remove_node(const std::string& v) {
        if (!nodes_.count(v)) return;
        if (compound_) {
            children_[parent_[v]].erase(v);
            parent_.erase(v);
            for (const std::string& child : children(v)) set_parent(child, "");
            children_.erase(v);
        }
        for (const EdgeObj& e : in_edges(v)) remove_edge(e);
        for (const EdgeObj& e : out_edges(v)) remove_edge(e);
        nodes_.erase(v);
        node_seq_.erase(std::find(node_seq_.begin(), node_seq_.end(), v));
    }

    // ---- nesting; an empty parent means "top level"
    void set_parent(const std::string& v, const std::string& parent) {
        if (!compound_) throw std::logic_error("Cannot set parent in a non-compound graph");
        std::string p = parent.empty() ? root_key() : parent;
        if (!parent.empty()) {
            for (std::string a = parent; !a.empty(); a = this->parent(a))
                if (a == v) throw std::invalid_argument("Setting " + parent + " as parent of " + v + " would create a cycle");
            set_node(parent);
        }
        set_node(v);
        children_[parent_[v]].erase(v);
        parent_[v] = p;
        children_[p].increment(v);
    }
    std::string parent(const std::string& v) const {
        if (!compound_) return {};
        const auto it = parent_.find(v);
        if (it == parent_.end() || it->second == root_key()) return {};
        return it->second;
    }
    // Children of `v`; an empty `v` lists the top level.
    std::vector<std::string> children(const std::string& v = {}) const {
        if (compound_) {
            const auto it = children_.find(v.empty() ? root_key() : v);
            return it == children_.end() ? std::vector<std::string>() : it->second.keys();
        }
        if (v.empty()) return nodes();
        return {};
    }
    bool has_children(const std::string& v) const {
        if (!compound_) return false;
        const auto it = children_.find(v);
        return it != children_.end() && it->second.size() != 0;
    }

    // ---- adjacency
    std::vector<std::string> predecessors(const std::string& v) const {
        const auto it = nodes_.find(v);
        return it == nodes_.end() ? std::vector<std::string>() : it->second.preds.keys();
    }
    std::vector<std::string> successors(const std::string& v) const {
        const auto it = nodes_.find(v);
        return it == nodes_.end() ? std::vector<std::string>() : it->second.sucs.keys();
    }
    std::vector<std::string> neighbors(const std::string& v) const {
        std::vector<std::string> out = predecessors(v);
        for (const std::string& s : successors(v))
            if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
        return out;
    }
    std::vector<std::string> sources() const {
        std::vector<std::string> out;
        for (const std::string& v : nodes())
            if (nodes_.at(v).preds.size() == 0) out.push_back(v);
        return out;
    }

    // ---- edges
    void set_edge(const std::string& v, const std::string& w, EdgePtr label, const std::string& name = {}) {
        if (!name.empty() && !multigraph_) throw std::logic_error("Cannot set a named edge when isMultigraph = false");
        const std::string id = edge_id(v, w, name);
        auto it = edges_.find(id);
        if (it != edges_.end()) {
            it->second.label = std::move(label);
            return;
        }
        set_node(v);
        set_node(w);
        EdgeObj obj{v, w, name};
        if (!directed_ && obj.v > obj.w) std::swap(obj.v, obj.w);
        edges_[id] = Entry{obj, std::move(label)};
        edge_seq_.push_back(id);
        nodes_[obj.w].preds.increment(obj.v);
        nodes_[obj.v].sucs.increment(obj.w);
        nodes_[obj.w].in.push_back(id);
        nodes_[obj.v].out.push_back(id);
    }
    void set_edge(const EdgeObj& e, EdgePtr label) { set_edge(e.v, e.w, std::move(label), e.name); }

    EdgePtr edge(const EdgeObj& e) const { return edge(e.v, e.w, e.name); }
    EdgePtr edge(const std::string& v, const std::string& w, const std::string& name = {}) const {
        const auto it = edges_.find(edge_id(v, w, name));
        return it == edges_.end() ? EdgePtr() : it->second.label;
    }
    bool has_edge(const std::string& v, const std::string& w, const std::string& name = {}) const {
        return edges_.count(edge_id(v, w, name)) != 0;
    }
    size_t edge_count() const { return edges_.size(); }

    void remove_edge(const EdgeObj& e) { remove_edge(e.v, e.w, e.name); }
    void remove_edge(const std::string& v, const std::string& w, const std::string& name = {}) {
        const std::string id = edge_id(v, w, name);
        const auto it = edges_.find(id);
        if (it == edges_.end()) return;
        const EdgeObj obj = it->second.obj;
        edges_.erase(it);
        edge_seq_.erase(std::find(edge_seq_.begin(), edge_seq_.end(), id));
        nodes_[obj.w].preds.decrement(obj.v);
        nodes_[obj.v].sucs.decrement(obj.w);
        erase_value(nodes_[obj.w].in, id);
        erase_value(nodes_[obj.v].out, id);
    }

    std::vector<EdgeObj> edges() const {
        std::vector<EdgeObj> out;
        out.reserve(edge_seq_.size());
        for (const std::string& id : edge_seq_) out.push_back(edges_.at(id).obj);
        return out;
    }
    std::vector<EdgeObj> in_edges(const std::string& v, const std::string& u = {}) const {
        return collect(v, true, u);
    }
    std::vector<EdgeObj> out_edges(const std::string& v, const std::string& w = {}) const {
        return collect(v, false, w);
    }
    std::vector<EdgeObj> node_edges(const std::string& v) const {
        std::vector<EdgeObj> out = in_edges(v);
        for (const EdgeObj& e : out_edges(v)) out.push_back(e);
        return out;
    }

private:
    struct NodeEntry {
        NodePtr label;
        std::vector<std::string> in;   // edge ids
        std::vector<std::string> out;  // edge ids
        KeyCounts preds;
        KeyCounts sucs;
    };
    struct Entry {
        EdgeObj obj;
        EdgePtr label;
    };

    static const std::string& root_key() {
        static const std::string key(1, '\0');
        return key;
    }
    static void erase_value(std::vector<std::string>& v, const std::string& x) {
        v.erase(std::find(v.begin(), v.end(), x));
    }

    void add_node(const std::string& v, NodePtr label) {
        nodes_[v].label = std::move(label);
        node_seq_.push_back(v);
        if (compound_) {
            parent_[v] = root_key();
            children_[v];
            children_[root_key()].increment(v);
        }
    }

    std::string edge_id(const std::string& v, const std::string& w, const std::string& name) const {
        const std::string* a = &v;
        const std::string* b = &w;
        if (!directed_ && v > w) std::swap(a, b);
        std::string id = *a;
        id += '\x01';
        id += *b;
        id += '\x01';
        id += name.empty() ? std::string(1, '\0') : name;
        return id;
    }

    std::vector<EdgeObj> collect(const std::string& v, bool incoming, const std::string& other) const {
        std::vector<EdgeObj> out;
        const auto it = nodes_.find(v);
        if (it == nodes_.end()) return out;
        for (const std::string& id : incoming ? it->second.in : it->second.out) {
            const EdgeObj& e = edges_.at(id).obj;
            if (other.empty() || (incoming ? e.v : e.w) == other) out.push_back(e);
        }
        return out;
    }

    bool directed_;
    bool multigraph_;
    bool compound_;
    std::shared_ptr<GraphLabel> label_;
    NodeFactory node_factory_;
    std::unordered_map<std::string, NodeEntry> nodes_;
    std::vector<std::string> node_seq_;
    std::unordered_map<std::string, std::string> parent_;
    std::unordered_map<std::string, KeyCounts> children_;
    std::unordered_map<std::string, Entry> edges_;
    std::vector<std::string> edge_seq_;
};

using LayoutGraph = Graph<NodeLabel, EdgeLabel>;

}  // namespace mermaid::dagre::detail

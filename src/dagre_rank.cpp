#include "dagre_rank.h"

#include <functional>
#include <limits>
#include <unordered_set>

namespace mermaid::dagre::detail {

double NetworkSimplexRanker::slack(const EdgeObj& e) const {
    return *g_.node(e.w)->rank - *g_.node(e.v)->rank - g_.edge(e)->minlen;
}

void NetworkSimplexRanker::run() {
    longest_path();
    build_feasible_tree();
    init_low_lim_values();
    init_cut_values();

    EdgeObj leaving;
    while (find_leaving_edge(leaving)) {
        const EdgeObj entering = find_entering_edge(leaving);
        exchange_edges(leaving, entering);
    }
}

// ---- initial ranking: sinks get 0, everything else as high as its edges allow

void NetworkSimplexRanker::longest_path() {
    std::unordered_set<std::string> visited;
    std::function<double(const std::string&)> visit = [&](const std::string& v) -> double {
        const NodeLabelPtr label = g_.node(v);
        if (visited.count(v)) return *label->rank;
        visited.insert(v);

        std::optional<double> rank;
        for (const EdgeObj& e : g_.out_edges(v)) {
            const double candidate = visit(e.w) - g_.edge(e)->minlen;
            if (!rank || candidate < *rank) rank = candidate;
        }
        label->rank = rank.value_or(0);
        return *label->rank;
    };
    for (const std::string& v : g_.sources()) visit(v);
}

// ---- feasible tree: grow a tree of tight edges, shifting ranks when it gets stuck

void NetworkSimplexRanker::build_feasible_tree() {
    tree_ = std::make_unique<Tree>(false, false, false);
    const std::string start = g_.nodes().front();
    const size_t size = g_.node_count();
    tree_->set_node(start, std::make_shared<NodeLabel>());

    while (grow_tight_tree() < size) {
        const EdgeObj edge = find_min_slack_edge();
        const double delta = tree_->has_node(edge.v) ? slack(edge) : -slack(edge);
        for (const std::string& v : tree_->nodes()) *g_.node(v)->rank += delta;
    }
}

size_t NetworkSimplexRanker::grow_tight_tree() {
    std::function<void(const std::string&)> visit = [&](const std::string& v) {
        for (const EdgeObj& e : g_.node_edges(v)) {
            const std::string& w = v == e.v ? e.w : e.v;
            if (!tree_->has_node(w) && slack(e) == 0) {
                tree_->set_node(w, std::make_shared<NodeLabel>());
                tree_->set_edge(v, w, std::make_shared<EdgeLabel>());
                visit(w);
            }
        }
    };
    for (const std::string& v : tree_->nodes()) visit(v);
    return tree_->node_count();
}

EdgeObj NetworkSimplexRanker::find_min_slack_edge() const {
    std::optional<EdgeObj> best;
    double best_slack = 0;
    for (const EdgeObj& e : g_.edges()) {
        if (tree_->has_node(e.v) == tree_->has_node(e.w)) continue;
        const double s = slack(e);
        if (!best || s < best_slack) {
            best = e;
            best_slack = s;
        }
    }
    if (!best) throw std::logic_error("network simplex: the graph is not connected");
    return *best;
}

// ---- tree bookkeeping

std::vector<std::string> NetworkSimplexRanker::postorder(const std::string& root) const {
    std::vector<std::string> out;
    std::unordered_set<std::string> visited;
    std::function<void(const std::string&)> visit = [&](const std::string& v) {
        if (!visited.insert(v).second) return;
        for (const std::string& w : tree_->neighbors(v)) visit(w);
        out.push_back(v);
    };
    visit(root);
    return out;
}

std::vector<std::string> NetworkSimplexRanker::preorder(const std::string& root) const {
    std::vector<std::string> out;
    std::unordered_set<std::string> visited;
    std::function<void(const std::string&)> visit = [&](const std::string& v) {
        if (!visited.insert(v).second) return;
        out.push_back(v);
        for (const std::string& w : tree_->neighbors(v)) visit(w);
    };
    visit(root);
    return out;
}

int NetworkSimplexRanker::assign_low_lim(const std::string& v, const std::string& parent, int next_lim,
                                         std::unordered_set<std::string>& visited) {
    const int low = next_lim;
    const NodeLabelPtr label = tree_->node(v);
    visited.insert(v);
    for (const std::string& w : tree_->neighbors(v)) {
        if (!visited.count(w)) next_lim = assign_low_lim(w, v, next_lim, visited);
    }
    label->low = low;
    label->lim = next_lim++;
    label->parent = parent;
    return next_lim;
}

void NetworkSimplexRanker::init_low_lim_values() {
    std::unordered_set<std::string> visited;
    assign_low_lim(tree_->nodes().front(), "", 1, visited);
}

void NetworkSimplexRanker::init_cut_values() {
    std::vector<std::string> order = postorder(tree_->nodes().front());
    order.pop_back();
    for (const std::string& v : order) {
        const std::string parent = tree_->node(v)->parent;
        tree_->edge(v, parent)->cutvalue = calc_cut_value(v);
    }
}

double NetworkSimplexRanker::calc_cut_value(const std::string& child) const {
    const std::string parent = tree_->node(child)->parent;
    bool child_is_tail = true;
    EdgeLabelPtr graph_edge = g_.edge(child, parent);
    if (!graph_edge) {
        child_is_tail = false;
        graph_edge = g_.edge(parent, child);
    }

    double cut_value = graph_edge->weight;
    for (const EdgeObj& e : g_.node_edges(child)) {
        const bool is_out_edge = e.v == child;
        const std::string& other = is_out_edge ? e.w : e.v;
        if (other == parent) continue;

        const bool points_to_head = is_out_edge == child_is_tail;
        const double other_weight = g_.edge(e)->weight;
        cut_value += points_to_head ? other_weight : -other_weight;
        if (tree_->has_edge(child, other)) {
            const double other_cut = tree_->edge(child, other)->cutvalue;
            cut_value += points_to_head ? -other_cut : other_cut;
        }
    }
    return cut_value;
}

bool NetworkSimplexRanker::find_leaving_edge(EdgeObj& out) const {
    for (const EdgeObj& e : tree_->edges()) {
        if (tree_->edge(e)->cutvalue < 0) {
            out = e;
            return true;
        }
    }
    return false;
}

bool NetworkSimplexRanker::is_descendant(const NodeLabel& v, const NodeLabel& root) const {
    return root.low <= v.lim && v.lim <= root.lim;
}

EdgeObj NetworkSimplexRanker::find_entering_edge(const EdgeObj& leaving) const {
    std::string v = leaving.v;
    std::string w = leaving.w;
    if (!g_.has_edge(v, w)) std::swap(v, w);

    const NodeLabelPtr v_label = tree_->node(v);
    const NodeLabelPtr w_label = tree_->node(w);
    NodeLabelPtr tail = v_label;
    bool flip = false;
    if (v_label->lim > w_label->lim) {
        tail = w_label;
        flip = true;
    }

    std::optional<EdgeObj> best;
    double best_slack = 0;
    for (const EdgeObj& e : g_.edges()) {
        if (flip != is_descendant(*tree_->node(e.v), *tail) || flip == is_descendant(*tree_->node(e.w), *tail))
            continue;
        const double s = slack(e);
        if (!best || s < best_slack) {
            best = e;
            best_slack = s;
        }
    }
    if (!best) throw std::logic_error("network simplex: no entering edge");
    return *best;
}

void NetworkSimplexRanker::exchange_edges(const EdgeObj& leaving, const EdgeObj& entering) {
    tree_->remove_edge(leaving.v, leaving.w);
    tree_->set_edge(entering.v, entering.w, std::make_shared<EdgeLabel>());
    init_low_lim_values();
    init_cut_values();
    update_ranks();
}

void NetworkSimplexRanker::update_ranks() {
    // dagre looks for the first tree node whose *graph* label has no `parent`; graph labels
    // never have one, so that is the first node of the tree.
    const std::string root = tree_->nodes().front();
    std::vector<std::string> order = preorder(root);
    for (size_t i = 1; i < order.size(); ++i) {
        const std::string& v = order[i];
        const std::string parent = tree_->node(v)->parent;
        EdgeLabelPtr edge = g_.edge(v, parent);
        bool flipped = false;
        if (!edge) {
            edge = g_.edge(parent, v);
            flipped = true;
        }
        *g_.node(v)->rank = *g_.node(parent)->rank + (flipped ? edge->minlen : -edge->minlen);
    }
}

}  // namespace mermaid::dagre::detail

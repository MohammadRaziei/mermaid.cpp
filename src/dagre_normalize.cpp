#include "dagre_normalize.h"

#include <functional>
#include <unordered_map>

namespace mermaid::dagre::detail {

// ---------------------------------------------------------------- EdgeNormalizer

void EdgeNormalizer::run() {
    g_.graph().dummy_chains.clear();
    for (const EdgeObj& e : g_.edges()) normalize_edge(e);
}

void EdgeNormalizer::normalize_edge(const EdgeObj& e) {
    std::string v = e.v;
    double v_rank = *g_.node(v)->rank;
    const std::string w = e.w;
    const double w_rank = *g_.node(w)->rank;
    const EdgeLabelPtr edge_label = g_.edge(e);
    const std::optional<double> label_rank = edge_label->label_rank;

    if (w_rank == v_rank + 1) return;

    g_.remove_edge(e);

    for (int i = 0; ++v_rank < w_rank; ++i) {
        edge_label->points = std::vector<Point>();
        auto attrs = std::make_shared<NodeLabel>();
        attrs->width = 0;
        attrs->height = 0;
        attrs->edge_label = edge_label;
        attrs->edge_obj = e;
        attrs->rank = v_rank;
        const std::string dummy = add_dummy_node(g_, ids_, Dummy::Edge, attrs, "_d");
        if (label_rank && v_rank == *label_rank) {
            attrs->width = edge_label->width;
            attrs->height = edge_label->height;
            attrs->dummy = Dummy::EdgeLabel;
            attrs->labelpos = edge_label->labelpos;
        }
        auto link = std::make_shared<EdgeLabel>();
        link->weight = edge_label->weight;
        g_.set_edge(v, dummy, link, e.name);
        if (i == 0) g_.graph().dummy_chains.push_back(dummy);
        v = dummy;
    }
    auto last = std::make_shared<EdgeLabel>();
    last->weight = edge_label->weight;
    g_.set_edge(v, w, last, e.name);
}

void EdgeNormalizer::undo() {
    for (const std::string& chain_start : g_.graph().dummy_chains) {
        std::string v = chain_start;
        NodeLabelPtr node = g_.node(v);
        const EdgeLabelPtr original = node->edge_label;
        g_.set_edge(node->edge_obj, original);
        while (node->dummy != Dummy::None) {
            const std::string w = g_.successors(v).front();
            g_.remove_node(v);
            original->points->push_back({*node->x, *node->y});
            if (node->dummy == Dummy::EdgeLabel) {
                original->x = node->x;
                original->y = node->y;
                original->width = node->width;
                original->height = node->height;
            }
            v = w;
            node = g_.node(v);
        }
    }
}

// ---------------------------------------------------------------- ParentDummyChains

namespace {

struct PostorderNum {
    int low = 0;
    int lim = 0;
};

struct Path {
    std::vector<std::string> nodes;  // "" stands for "no parent" (top level)
    std::string lca;
};

// dagre compares a rank that may be undefined: such a comparison is simply false.
bool is_below(const std::optional<int>& value, double rank) { return value && *value < rank; }
bool is_at_most(const std::optional<int>& value, double rank) { return value && *value <= rank; }

}  // namespace

void ParentDummyChains::run() {
    std::unordered_map<std::string, PostorderNum> nums;
    int lim = 0;
    std::function<void(const std::string&)> number = [&](const std::string& v) {
        const int low = lim;
        for (const std::string& child : g_.children(v)) number(child);
        nums[v] = {low, lim++};
    };
    for (const std::string& v : g_.children()) number(v);

    auto find_path = [&](const std::string& v, const std::string& w) {
        Path path;
        std::vector<std::string> w_path;
        const int low = std::min(nums.at(v).low, nums.at(w).low);
        const int high = std::max(nums.at(v).lim, nums.at(w).lim);

        std::string parent = v;
        do {
            parent = g_.parent(parent);
            path.nodes.push_back(parent);
        } while (!parent.empty() && (nums.at(parent).low > low || high > nums.at(parent).lim));
        path.lca = parent;

        parent = w;
        while ((parent = g_.parent(parent)) != path.lca) w_path.push_back(parent);

        path.nodes.insert(path.nodes.end(), w_path.rbegin(), w_path.rend());
        return path;
    };

    for (const std::string& chain_start : g_.graph().dummy_chains) {
        std::string v = chain_start;
        NodeLabelPtr node = g_.node(v);
        const EdgeObj edge_obj = node->edge_obj;
        const Path path = find_path(edge_obj.v, edge_obj.w);
        size_t path_idx = 0;
        std::string path_v = path.nodes[path_idx];
        bool ascending = true;

        while (v != edge_obj.w) {
            node = g_.node(v);
            const double rank = *node->rank;
            if (ascending) {
                while ((path_v = path.nodes[path_idx]) != path.lca && is_below(g_.node(path_v)->max_rank, rank))
                    ++path_idx;
                if (path_v == path.lca) ascending = false;
            }
            if (!ascending) {
                while (path_idx + 1 < path.nodes.size() &&
                       is_at_most(g_.node(path_v = path.nodes[path_idx + 1])->min_rank, rank))
                    ++path_idx;
                path_v = path.nodes[path_idx];
            }
            g_.set_parent(v, path_v);
            v = g_.successors(v).front();
        }
    }
}

// ---------------------------------------------------------------- BorderSegments

void BorderSegments::run() {
    for (const std::string& v : g_.children()) visit(v);
}

void BorderSegments::visit(const std::string& v) {
    const std::vector<std::string> children = g_.children(v);
    const NodeLabelPtr node = g_.node(v);
    for (const std::string& child : children) visit(child);

    if (node->min_rank) {
        node->border_left.clear();
        node->border_right.clear();
        for (int rank = *node->min_rank, top = *node->max_rank + 1; rank < top; ++rank) {
            add_border(true, v, *node, rank);
            add_border(false, v, *node, rank);
        }
    }
}

void BorderSegments::add_border(bool left, const std::string& compound, NodeLabel& compound_label, int rank) {
    auto label = std::make_shared<NodeLabel>();
    label->width = 0;
    label->height = 0;
    label->rank = rank;
    label->border_side = left ? BorderSide::Left : BorderSide::Right;

    auto& sides = left ? compound_label.border_left : compound_label.border_right;
    const auto previous = sides.find(rank - 1);
    const std::string current = add_dummy_node(g_, ids_, Dummy::Border, label, left ? "_bl" : "_br");
    sides[rank] = current;
    g_.set_parent(current, compound);
    if (previous != sides.end()) {
        auto edge = std::make_shared<EdgeLabel>();
        edge->weight = 1;
        g_.set_edge(previous->second, current, edge);
    }
}

}  // namespace mermaid::dagre::detail

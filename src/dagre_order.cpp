#include "dagre_order.h"

#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace mermaid::dagre::detail {

// ---------------------------------------------------------------- barycenters

std::vector<Barycenter> barycenters(const LayoutGraph& g, const std::vector<std::string>& movable) {
    std::vector<Barycenter> out;
    out.reserve(movable.size());
    for (const std::string& v : movable) {
        const std::vector<EdgeObj> incoming = g.in_edges(v);
        Barycenter entry;
        entry.v = v;
        if (!incoming.empty()) {
            double sum = 0;
            double weight = 0;
            for (const EdgeObj& e : incoming) {
                const double w = g.edge(e)->weight;
                sum += w * g.node(e.v)->order.value_or(0);
                weight += w;
            }
            entry.value = sum / weight;
            entry.weight = weight;
        }
        out.push_back(entry);
    }
    return out;
}

// ---------------------------------------------------------------- conflict resolution

namespace {

struct Mapped {
    int indegree = 0;
    std::vector<int> in;
    std::vector<int> out;
    std::vector<std::string> vs;
    int i = 0;
    std::optional<double> barycenter;
    double weight = 0;
    bool merged = false;
};

void merge_entries(Mapped& target, Mapped& source) {
    double sum = 0;
    double weight = 0;
    if (target.weight != 0) {
        sum += *target.barycenter * target.weight;
        weight += target.weight;
    }
    if (source.weight != 0) {
        sum += *source.barycenter * source.weight;
        weight += source.weight;
    }
    std::vector<std::string> joined = source.vs;
    joined.insert(joined.end(), target.vs.begin(), target.vs.end());
    target.vs = std::move(joined);
    target.barycenter = sum / weight;
    target.weight = weight;
    target.i = std::min(source.i, target.i);
    source.merged = true;
}

}  // namespace

std::vector<SortEntry> resolve_conflicts(const std::vector<Barycenter>& entries, const ConstraintGraph& cg) {
    std::vector<Mapped> mapped(entries.size());
    std::unordered_map<std::string, int> index_of;
    std::vector<std::string> keys;
    for (size_t i = 0; i < entries.size(); ++i) {
        Mapped& m = mapped[i];
        m.i = static_cast<int>(i);
        m.vs = {entries[i].v};
        if (entries[i].value) {
            m.barycenter = entries[i].value;
            m.weight = entries[i].weight;
        }
        index_of[entries[i].v] = static_cast<int>(i);
        keys.push_back(entries[i].v);
    }

    for (const EdgeObj& e : cg.edges()) {
        const auto v = index_of.find(e.v);
        const auto w = index_of.find(e.w);
        if (v == index_of.end() || w == index_of.end()) continue;
        ++mapped[static_cast<size_t>(w->second)].indegree;
        mapped[static_cast<size_t>(v->second)].out.push_back(w->second);
    }

    std::vector<int> source_set;
    for (const std::string& key : js_key_order(keys)) {
        const int idx = index_of.at(key);
        if (mapped[static_cast<size_t>(idx)].indegree == 0) source_set.push_back(idx);
    }

    std::vector<int> processed;
    while (!source_set.empty()) {
        const int entry = source_set.back();
        source_set.pop_back();
        processed.push_back(entry);

        Mapped& v = mapped[static_cast<size_t>(entry)];
        std::reverse(v.in.begin(), v.in.end());
        for (const int u_idx : std::vector<int>(v.in)) {
            Mapped& u = mapped[static_cast<size_t>(u_idx)];
            if (u.merged) continue;
            if (!u.barycenter || !v.barycenter || *u.barycenter >= *v.barycenter) merge_entries(v, u);
        }
        for (const int w_idx : v.out) {
            Mapped& w = mapped[static_cast<size_t>(w_idx)];
            w.in.push_back(entry);
            if (--w.indegree == 0) source_set.push_back(w_idx);
        }
    }

    std::vector<SortEntry> out;
    for (const int idx : processed) {
        Mapped& m = mapped[static_cast<size_t>(idx)];
        if (m.merged) continue;
        SortEntry entry;
        entry.vs = m.vs;
        entry.i = m.i;
        entry.barycenter = m.barycenter;
        entry.weight = m.weight;
        out.push_back(std::move(entry));
    }
    return out;
}

// ---------------------------------------------------------------- sorting entries

SortResult sort_entries(std::vector<SortEntry> entries, bool bias_right) {
    std::vector<SortEntry> sortable;
    std::vector<SortEntry> unsortable;
    for (SortEntry& e : entries) (e.barycenter ? sortable : unsortable).push_back(std::move(e));

    // Largest original position first, so the one to place next is at the back.
    std::stable_sort(unsortable.begin(), unsortable.end(),
                     [](const SortEntry& a, const SortEntry& b) { return -a.i < -b.i; });

    auto compare = [bias_right](const SortEntry& a, const SortEntry& b) {
        if (*a.barycenter < *b.barycenter) return -1;
        if (*a.barycenter > *b.barycenter) return 1;
        return !bias_right ? a.i - b.i : b.i - a.i;
    };
    std::stable_sort(sortable.begin(), sortable.end(),
                     [&](const SortEntry& a, const SortEntry& b) { return compare(a, b) < 0; });

    std::vector<std::string> vs;
    int index = 0;
    auto consume_unsortable = [&]() {
        while (!unsortable.empty() && unsortable.back().i <= index) {
            const SortEntry& last = unsortable.back();
            vs.insert(vs.end(), last.vs.begin(), last.vs.end());
            unsortable.pop_back();
            ++index;
        }
    };

    double sum = 0;
    double weight = 0;
    consume_unsortable();
    for (const SortEntry& entry : sortable) {
        index += static_cast<int>(entry.vs.size());
        vs.insert(vs.end(), entry.vs.begin(), entry.vs.end());
        sum += *entry.barycenter * entry.weight;
        weight += entry.weight;
        consume_unsortable();
    }

    SortResult result;
    result.vs = std::move(vs);
    if (weight != 0) {
        result.barycenter = sum / weight;
        result.weight = weight;
    }
    return result;
}

// ---------------------------------------------------------------- subgraphs

SortResult sort_subgraph(const LayoutGraph& g, const std::string& v, const ConstraintGraph& cg, bool bias_right) {
    std::vector<std::string> movable = g.children(v);
    const NodeLabelPtr node = g.node(v);
    const std::string bl = node ? node->layer_border_left : std::string();
    const std::string br = node ? node->layer_border_right : std::string();
    if (!bl.empty()) {
        movable.erase(std::remove_if(movable.begin(), movable.end(),
                                     [&](const std::string& w) { return w == bl || w == br; }),
                      movable.end());
    }

    std::vector<Barycenter> entries = barycenters(g, movable);
    std::unordered_map<std::string, SortResult> subgraphs;
    for (Barycenter& entry : entries) {
        if (!g.has_children(entry.v)) continue;
        SortResult sub = sort_subgraph(g, entry.v, cg, bias_right);
        if (sub.barycenter) {
            if (entry.value) {
                entry.value = (*entry.value * entry.weight + *sub.barycenter * sub.weight) / (entry.weight + sub.weight);
                entry.weight += sub.weight;
            } else {
                entry.value = sub.barycenter;
                entry.weight = sub.weight;
            }
        }
        subgraphs[entry.v] = std::move(sub);
    }

    std::vector<SortEntry> resolved = resolve_conflicts(entries, cg);
    for (SortEntry& entry : resolved) {
        std::vector<std::string> expanded;
        for (const std::string& member : entry.vs) {
            const auto it = subgraphs.find(member);
            if (it != subgraphs.end())
                expanded.insert(expanded.end(), it->second.vs.begin(), it->second.vs.end());
            else
                expanded.push_back(member);
        }
        entry.vs = std::move(expanded);
    }

    SortResult result = sort_entries(std::move(resolved), bias_right);
    if (!bl.empty()) {
        std::vector<std::string> framed{bl};
        framed.insert(framed.end(), result.vs.begin(), result.vs.end());
        framed.push_back(br);
        result.vs = std::move(framed);

        const std::vector<std::string> bl_preds = g.predecessors(bl);
        if (!bl_preds.empty()) {
            const NodeLabelPtr bl_pred = g.node(bl_preds.front());
            const NodeLabelPtr br_pred = g.node(g.predecessors(br).front());
            if (!result.barycenter) {
                result.barycenter = 0;
                result.weight = 0;
            }
            result.barycenter = (*result.barycenter * result.weight + bl_pred->order.value_or(0) +
                                 br_pred->order.value_or(0)) /
                                (result.weight + 2);
            result.weight += 2;
        }
    }
    return result;
}

void add_subgraph_constraints(const LayoutGraph& g, ConstraintGraph& cg, const std::vector<std::string>& vs) {
    std::unordered_map<std::string, std::string> prev;
    std::string root_prev;
    for (const std::string& v : vs) {
        std::string child = g.parent(v);
        while (!child.empty()) {
            const std::string parent = g.parent(child);
            std::string prev_child;
            if (!parent.empty()) {
                prev_child = prev[parent];
                prev[parent] = child;
            } else {
                prev_child = root_prev;
                root_prev = child;
            }
            if (!prev_child.empty() && prev_child != child) {
                cg.set_edge(prev_child, child, nullptr);
                break;
            }
            child = parent;
        }
    }
}

// ---------------------------------------------------------------- crossings

namespace {

double two_layer_cross_count(const LayoutGraph& g, const std::vector<std::string>& north,
                             const std::vector<std::string>& south) {
    std::unordered_map<std::string, int> south_pos;
    for (size_t i = 0; i < south.size(); ++i) south_pos[south[i]] = static_cast<int>(i);

    struct Entry {
        int pos;
        double weight;
    };
    std::vector<Entry> south_entries;
    for (const std::string& v : north) {
        std::vector<Entry> mine;
        for (const EdgeObj& e : g.out_edges(v)) {
            const auto pos = south_pos.find(e.w);
            if (pos == south_pos.end()) continue;
            mine.push_back({pos->second, g.edge(e)->weight});
        }
        std::stable_sort(mine.begin(), mine.end(), [](const Entry& a, const Entry& b) { return a.pos < b.pos; });
        south_entries.insert(south_entries.end(), mine.begin(), mine.end());
    }

    size_t first_index = 1;
    while (first_index < south.size()) first_index <<= 1;
    const size_t tree_size = 2 * first_index - 1;
    first_index -= 1;
    std::vector<double> tree(tree_size, 0.0);

    double cc = 0;
    for (const Entry& entry : south_entries) {
        size_t index = static_cast<size_t>(entry.pos) + first_index;
        tree[index] += entry.weight;
        double weight_sum = 0;
        while (index > 0) {
            if (index % 2) weight_sum += tree[index + 1];
            index = (index - 1) >> 1;
            tree[index] += entry.weight;
        }
        cc += entry.weight * weight_sum;
    }
    return cc;
}

}  // namespace

double cross_count(const LayoutGraph& g, const Layers& layering) {
    double cc = 0;
    for (size_t i = 1; i < layering.size(); ++i) cc += two_layer_cross_count(g, layering[i - 1], layering[i]);
    return cc;
}

// ---------------------------------------------------------------- Orderer

LayoutGraph Orderer::build_layer_graph(int rank, Direction direction) {
    std::string root;
    do {
        root = ids_.next("_root");
    } while (g_.has_node(root));

    LayoutGraph result(true, false, true);
    auto label = std::make_shared<GraphLabel>();
    label->root = root;
    result.set_graph(label);
    result.set_default_node_label([this](const std::string& v) { return g_.node(v); });

    for (const std::string& v : g_.nodes()) {
        const NodeLabelPtr node = g_.node(v);
        const std::string parent = g_.parent(v);
        const bool here = (node->rank && *node->rank == rank) ||
                          (node->min_rank && node->max_rank && *node->min_rank <= rank && rank <= *node->max_rank);
        if (!here) continue;

        result.set_node(v);
        result.set_parent(v, parent.empty() ? root : parent);
        for (const EdgeObj& e : direction == Direction::In ? g_.in_edges(v) : g_.out_edges(v)) {
            const std::string& u = e.v == v ? e.w : e.v;
            const EdgeLabelPtr existing = result.edge(u, v);
            auto merged = std::make_shared<EdgeLabel>();
            merged->weight = g_.edge(e)->weight + (existing ? existing->weight : 0);
            result.set_edge(u, v, merged);
        }
        if (node->min_rank) {
            auto borders = std::make_shared<NodeLabel>();
            const auto left = node->border_left.find(rank);
            const auto right = node->border_right.find(rank);
            if (left != node->border_left.end()) borders->layer_border_left = left->second;
            if (right != node->border_right.end()) borders->layer_border_right = right->second;
            result.set_node(v, borders);
        }
    }
    return result;
}

std::vector<LayoutGraph> Orderer::build_layer_graphs(const std::vector<int>& ranks, Direction direction) {
    std::vector<LayoutGraph> out;
    out.reserve(ranks.size());
    for (const int rank : ranks) out.push_back(build_layer_graph(rank, direction));
    return out;
}

void Orderer::sweep(std::vector<LayoutGraph>& layer_graphs, bool bias_right) {
    ConstraintGraph cg(true, false, false);
    for (LayoutGraph& lg : layer_graphs) {
        const SortResult sorted = sort_subgraph(lg, lg.graph().root, cg, bias_right);
        for (size_t i = 0; i < sorted.vs.size(); ++i) lg.node(sorted.vs[i])->order = static_cast<int>(i);
        add_subgraph_constraints(lg, cg, sorted.vs);
    }
}

Layers Orderer::initial_order() const {
    std::vector<std::string> simple;
    for (const std::string& v : g_.nodes())
        if (!g_.has_children(v)) simple.push_back(v);

    double top = 0;
    for (const std::string& v : simple) top = std::max(top, *g_.node(v)->rank);
    Layers layers(static_cast<size_t>(top) + 1);

    std::stable_sort(simple.begin(), simple.end(),
                     [&](const std::string& a, const std::string& b) { return *g_.node(a)->rank < *g_.node(b)->rank; });

    std::unordered_set<std::string> visited;
    std::function<void(const std::string&)> visit = [&](const std::string& v) {
        if (!visited.insert(v).second) return;
        layers[static_cast<size_t>(*g_.node(v)->rank)].push_back(v);
        for (const std::string& w : g_.successors(v)) visit(w);
    };
    for (const std::string& v : simple) visit(v);
    return layers;
}

void Orderer::assign_order(const Layers& layering) {
    for (const auto& layer : layering)
        for (size_t i = 0; i < layer.size(); ++i) g_.node(layer[i])->order = static_cast<int>(i);
}

void Orderer::run() {
    const std::optional<int> top = max_rank(g_);
    if (!top) return;

    std::vector<int> down_ranks;
    for (int r = 1; r <= *top; ++r) down_ranks.push_back(r);
    std::vector<int> up_ranks;
    for (int r = *top - 1; r >= 0; --r) up_ranks.push_back(r);
    std::vector<LayoutGraph> down = build_layer_graphs(down_ranks, Direction::In);
    std::vector<LayoutGraph> up = build_layer_graphs(up_ranks, Direction::Out);

    Layers layering = initial_order();
    assign_order(layering);

    double best_cc = std::numeric_limits<double>::infinity();
    Layers best;
    for (int i = 0, last_best = 0; last_best < 4; ++i, ++last_best) {
        sweep(i % 2 ? down : up, i % 4 >= 2);
        layering = build_layer_matrix(g_);
        const double cc = cross_count(g_, layering);
        if (cc < best_cc) {
            last_best = 0;
            best = layering;
            best_cc = cc;
        }
    }
    assign_order(best);
}

}  // namespace mermaid::dagre::detail

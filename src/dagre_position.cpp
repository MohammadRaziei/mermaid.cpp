#include "dagre_position.h"

#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace mermaid::dagre::detail {

namespace {

using Coords = std::unordered_map<std::string, double>;
using Conflicts = std::unordered_map<std::string, std::unordered_set<std::string>>;
using NeighborFn = std::function<std::vector<std::string>(const std::string&)>;

struct Unit {};
using BlockGraph = Graph<Unit, double>;

struct Alignment {
    std::unordered_map<std::string, std::string> root;
    std::unordered_map<std::string, std::string> align;
};

class BrandesKoepf {
public:
    explicit BrandesKoepf(const LayoutGraph& g) : g_(g) {}

    Coords position_x() {
        const Layers layering = build_layer_matrix(g_);
        Conflicts conflicts = type1_conflicts(layering);
        for (auto& entry : type2_conflicts(layering))
            for (const std::string& w : entry.second) conflicts[entry.first].insert(w);

        std::array<Coords, 4> xss;  // ul, ur, dl, dr
        for (int vert = 0; vert < 2; ++vert) {
            const bool up = vert == 0;
            Layers adjusted = layering;
            if (!up) std::reverse(adjusted.begin(), adjusted.end());
            for (int horiz = 0; horiz < 2; ++horiz) {
                const bool right = horiz == 1;
                if (right)
                    for (auto& layer : adjusted) std::reverse(layer.begin(), layer.end());

                NeighborFn neighbors = up ? NeighborFn([this](const std::string& v) { return g_.predecessors(v); })
                                          : NeighborFn([this](const std::string& v) { return g_.successors(v); });
                const Alignment alignment = vertical_alignment(adjusted, conflicts, neighbors);
                Coords xs = horizontal_compaction(adjusted, alignment, right);
                if (right)
                    for (auto& entry : xs) entry.second = -entry.second;
                xss[static_cast<size_t>(vert * 2 + horiz)] = std::move(xs);
            }
        }

        const size_t smallest = find_smallest_width_alignment(xss);
        align_coordinates(xss, smallest);
        return balance(xss);
    }

private:
    // ---- conflicts: segments that must not be straightened because they cross

    static void add_conflict(Conflicts& conflicts, std::string v, std::string w) {
        if (v > w) std::swap(v, w);
        conflicts[v].insert(w);
    }
    static bool has_conflict(const Conflicts& conflicts, std::string v, std::string w) {
        if (v > w) std::swap(v, w);
        const auto it = conflicts.find(v);
        return it != conflicts.end() && it->second.count(w) != 0;
    }

    std::string other_inner_segment_node(const std::string& v) const {
        if (g_.node(v)->dummy == Dummy::None) return {};
        for (const std::string& u : g_.predecessors(v))
            if (g_.node(u)->dummy != Dummy::None) return u;
        return {};
    }

    Conflicts type1_conflicts(const Layers& layering) const {
        Conflicts conflicts;
        for (size_t li = 1; li < layering.size(); ++li) {
            const auto& prev_layer = layering[li - 1];
            const auto& layer = layering[li];
            int k0 = 0;
            size_t scan_pos = 0;
            const int prev_length = static_cast<int>(prev_layer.size());
            const std::string last_node = layer.empty() ? std::string() : layer.back();
            for (size_t i = 0; i < layer.size(); ++i) {
                const std::string& v = layer[i];
                const std::string w = other_inner_segment_node(v);
                const int k1 = !w.empty() ? *g_.node(w)->order : prev_length;
                if (!w.empty() || v == last_node) {
                    for (size_t s = scan_pos; s <= i; ++s) {
                        const std::string& scan_node = layer[s];
                        for (const std::string& u : g_.predecessors(scan_node)) {
                            const NodeLabelPtr u_label = g_.node(u);
                            const int u_pos = *u_label->order;
                            if ((u_pos < k0 || k1 < u_pos) &&
                                !(u_label->dummy != Dummy::None && g_.node(scan_node)->dummy != Dummy::None))
                                add_conflict(conflicts, u, scan_node);
                        }
                    }
                    scan_pos = i + 1;
                    k0 = k1;
                }
            }
        }
        return conflicts;
    }

    Conflicts type2_conflicts(const Layers& layering) const {
        Conflicts conflicts;
        const double undefined = std::nan("");

        auto scan = [&](const std::vector<std::string>& south, size_t south_pos, size_t south_end,
                        double prev_north_border, double next_north_border) {
            for (size_t i = south_pos; i < south_end; ++i) {
                const std::string& v = south[i];
                if (g_.node(v)->dummy == Dummy::None) continue;
                for (const std::string& u : g_.predecessors(v)) {
                    const NodeLabelPtr u_node = g_.node(u);
                    if (u_node->dummy != Dummy::None &&
                        (*u_node->order < prev_north_border || *u_node->order > next_north_border))
                        add_conflict(conflicts, u, v);
                }
            }
        };

        for (size_t li = 1; li < layering.size(); ++li) {
            const auto& north = layering[li - 1];
            const auto& south = layering[li];
            double prev_north_pos = -1;
            double next_north_pos = undefined;
            size_t south_pos = 0;
            for (size_t lookahead = 0; lookahead < south.size(); ++lookahead) {
                const std::string& v = south[lookahead];
                if (g_.node(v)->dummy == Dummy::Border) {
                    const std::vector<std::string> preds = g_.predecessors(v);
                    if (!preds.empty()) {
                        next_north_pos = *g_.node(preds.front())->order;
                        scan(south, south_pos, lookahead, prev_north_pos, next_north_pos);
                        south_pos = lookahead;
                        prev_north_pos = next_north_pos;
                    }
                }
                scan(south, south_pos, south.size(), next_north_pos, static_cast<double>(north.size()));
            }
        }
        return conflicts;
    }

    // ---- vertical alignment: chain each node to its median neighbour(s)

    Alignment vertical_alignment(const Layers& layering, const Conflicts& conflicts,
                                 const NeighborFn& neighbors) const {
        Alignment a;
        std::unordered_map<std::string, int> pos;
        for (const auto& layer : layering)
            for (size_t order = 0; order < layer.size(); ++order) {
                a.root[layer[order]] = layer[order];
                a.align[layer[order]] = layer[order];
                pos[layer[order]] = static_cast<int>(order);
            }

        for (const auto& layer : layering) {
            int prev_idx = -1;
            for (const std::string& v : layer) {
                std::vector<std::string> ws = neighbors(v);
                if (ws.empty()) continue;
                std::stable_sort(ws.begin(), ws.end(),
                                 [&](const std::string& x, const std::string& y) { return pos.at(x) < pos.at(y); });
                const double mp = (static_cast<double>(ws.size()) - 1) / 2;
                for (int i = static_cast<int>(std::floor(mp)), il = static_cast<int>(std::ceil(mp)); i <= il; ++i) {
                    const std::string& w = ws[static_cast<size_t>(i)];
                    if (a.align[v] == v && prev_idx < pos.at(w) && !has_conflict(conflicts, v, w)) {
                        a.align[w] = v;
                        a.align[v] = a.root[v] = a.root[w];
                        prev_idx = pos.at(w);
                    }
                }
            }
        }
        return a;
    }

    // ---- horizontal compaction: place the blocks as far left as the separations allow

    double separation(const std::string& v, const std::string& w, bool reverse_sep) const {
        const GraphLabel& label = g_.graph();
        const NodeLabelPtr v_label = g_.node(v);
        const NodeLabelPtr w_label = g_.node(w);
        double sum = 0;
        double delta = 0;

        sum += v_label->width / 2;
        if (v_label->labelpos) {
            if (*v_label->labelpos == LabelPos::Left) delta = -v_label->width / 2;
            else if (*v_label->labelpos == LabelPos::Right) delta = v_label->width / 2;
        }
        if (delta != 0) sum += reverse_sep ? delta : -delta;

        delta = 0;
        sum += (v_label->dummy != Dummy::None ? label.edgesep : label.nodesep) / 2;
        sum += (w_label->dummy != Dummy::None ? label.edgesep : label.nodesep) / 2;
        sum += w_label->width / 2;
        if (w_label->labelpos) {
            if (*w_label->labelpos == LabelPos::Left) delta = w_label->width / 2;
            else if (*w_label->labelpos == LabelPos::Right) delta = -w_label->width / 2;
        }
        if (delta != 0) sum += reverse_sep ? delta : -delta;
        return sum;
    }

    BlockGraph build_block_graph(const Layers& layering, const Alignment& a, bool reverse_sep) const {
        BlockGraph block(true, false, false);
        for (const auto& layer : layering) {
            std::string u;
            for (const std::string& v : layer) {
                const std::string& v_root = a.root.at(v);
                block.set_node(v_root);
                if (!u.empty()) {
                    const std::string& u_root = a.root.at(u);
                    const auto previous = block.edge(u_root, v_root);
                    block.set_edge(u_root, v_root,
                                   std::make_shared<double>(std::max(separation(v, u, reverse_sep), previous ? *previous : 0.0)));
                }
                u = v;
            }
        }
        return block;
    }

    Coords horizontal_compaction(const Layers& layering, const Alignment& a, bool reverse_sep) const {
        Coords xs;
        BlockGraph block = build_block_graph(layering, a, reverse_sep);
        const BorderSide border_side = reverse_sep ? BorderSide::Left : BorderSide::Right;

        auto iterate = [&](const std::function<void(const std::string&)>& set_xs,
                           const std::function<std::vector<std::string>(const std::string&)>& next_nodes) {
            std::vector<std::string> stack = block.nodes();
            std::unordered_set<std::string> visited;
            while (!stack.empty()) {
                const std::string elem = stack.back();
                stack.pop_back();
                if (visited.count(elem)) {
                    set_xs(elem);
                } else {
                    visited.insert(elem);
                    stack.push_back(elem);
                    for (const std::string& n : next_nodes(elem)) stack.push_back(n);
                }
            }
        };

        auto pass1 = [&](const std::string& elem) {
            double best = 0;
            for (const EdgeObj& e : block.in_edges(elem)) best = std::max(best, xs[e.v] + *block.edge(e));
            xs[elem] = best;
        };
        auto pass2 = [&](const std::string& elem) {
            double least = std::numeric_limits<double>::infinity();
            for (const EdgeObj& e : block.out_edges(elem)) least = std::min(least, xs[e.w] - *block.edge(e));
            const NodeLabelPtr node = g_.node(elem);
            if (least != std::numeric_limits<double>::infinity() && node->border_side != border_side)
                xs[elem] = std::max(xs[elem], least);
        };

        iterate(pass1, [&](const std::string& v) { return block.predecessors(v); });
        iterate(pass2, [&](const std::string& v) { return block.successors(v); });

        for (const auto& entry : a.align) xs[entry.second] = xs[a.root.at(entry.second)];
        return xs;
    }

    // ---- combining the four layouts

    double width_of(const std::string& v) const { return g_.node(v)->width; }

    size_t find_smallest_width_alignment(const std::array<Coords, 4>& xss) const {
        size_t best = 0;
        double best_width = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < xss.size(); ++i) {
            double max = -std::numeric_limits<double>::infinity();
            double min = std::numeric_limits<double>::infinity();
            for (const auto& entry : xss[i]) {
                const double half = width_of(entry.first) / 2;
                max = std::max(entry.second + half, max);
                min = std::min(entry.second - half, min);
            }
            if (max - min < best_width) {
                best_width = max - min;
                best = i;
            }
        }
        return best;
    }

    static void align_coordinates(std::array<Coords, 4>& xss, size_t align_to) {
        double to_min = std::numeric_limits<double>::infinity();
        double to_max = -std::numeric_limits<double>::infinity();
        for (const auto& entry : xss[align_to]) {
            to_min = std::min(to_min, entry.second);
            to_max = std::max(to_max, entry.second);
        }
        for (size_t i = 0; i < xss.size(); ++i) {
            if (i == align_to) continue;
            const bool left = i % 2 == 0;
            double min = std::numeric_limits<double>::infinity();
            double max = -std::numeric_limits<double>::infinity();
            for (const auto& entry : xss[i]) {
                min = std::min(min, entry.second);
                max = std::max(max, entry.second);
            }
            const double delta = left ? to_min - min : to_max - max;
            if (delta != 0)
                for (auto& entry : xss[i]) entry.second += delta;
        }
    }

    static Coords balance(const std::array<Coords, 4>& xss) {
        Coords out;
        for (const auto& entry : xss[0]) {
            std::array<double, 4> values;
            for (size_t i = 0; i < 4; ++i) values[i] = xss[i].at(entry.first);
            std::sort(values.begin(), values.end());
            out[entry.first] = (values[1] + values[2]) / 2;
        }
        return out;
    }

    const LayoutGraph& g_;
};

void position_y(LayoutGraph& g) {
    const Layers layering = build_layer_matrix(g);
    const double rank_sep = g.graph().ranksep;
    double prev_y = 0;
    for (const auto& layer : layering) {
        double max_height = -std::numeric_limits<double>::infinity();
        for (const std::string& v : layer) max_height = std::max(max_height, g.node(v)->height);
        for (const std::string& v : layer) g.node(v)->y = prev_y + max_height / 2;
        prev_y += max_height + rank_sep;
    }
}

}  // namespace

void position(LayoutGraph& g) {
    LayoutGraph flat = as_non_compound_graph(g);
    position_y(flat);
    const Coords xs = BrandesKoepf(flat).position_x();
    for (const auto& entry : xs) flat.node(entry.first)->x = entry.second;
}

}  // namespace mermaid::dagre::detail

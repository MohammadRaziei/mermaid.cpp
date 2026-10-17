#include "layout_steps.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace mermaid::flowchart::detail {

// ---------------------------------------------------------------- CycleBreaker

std::vector<bool> CycleBreaker::reversed(int node_count,
                                         const std::vector<std::pair<int, int>>& edges) const {
    const size_t n = static_cast<size_t>(node_count);
    std::vector<std::vector<size_t>> out(n);
    for (size_t i = 0; i < edges.size(); ++i) out[static_cast<size_t>(edges[i].first)].push_back(i);

    enum class State { Fresh, OnStack, Done };
    std::vector<State> state(n, State::Fresh);
    std::vector<bool> flip(edges.size(), false);

    struct Frame {
        size_t node;
        size_t next;
    };
    for (size_t root = 0; root < n; ++root) {
        if (state[root] != State::Fresh) continue;
        std::vector<Frame> stack{{root, 0}};
        state[root] = State::OnStack;
        while (!stack.empty()) {
            Frame& f = stack.back();
            if (f.next == out[f.node].size()) {
                state[f.node] = State::Done;
                stack.pop_back();
                continue;
            }
            const size_t e = out[f.node][f.next++];
            const size_t to = static_cast<size_t>(edges[e].second);
            if (state[to] == State::OnStack) {
                flip[e] = true;
            } else if (state[to] == State::Fresh) {
                state[to] = State::OnStack;
                stack.push_back({to, 0});
            }
        }
    }
    return flip;
}

// ---------------------------------------------------------------------- Ranker

std::vector<int> Ranker::rank(int node_count, const std::vector<RankEdge>& edges) const {
    const size_t n = static_cast<size_t>(node_count);
    std::vector<std::vector<size_t>> out(n);
    std::vector<int> in_degree(n, 0), out_degree(n, 0);
    for (size_t i = 0; i < edges.size(); ++i) {
        out[static_cast<size_t>(edges[i].from)].push_back(i);
        ++in_degree[static_cast<size_t>(edges[i].to)];
        ++out_degree[static_cast<size_t>(edges[i].from)];
    }

    // Kahn's algorithm gives a topological order and the longest-path ranks.
    std::vector<int> rank(n, 0);
    std::vector<int> pending = in_degree;
    std::vector<size_t> order;
    for (size_t v = 0; v < n; ++v)
        if (pending[v] == 0) order.push_back(v);
    for (size_t head = 0; head < order.size(); ++head) {
        const size_t v = order[head];
        for (size_t e : out[v]) {
            const size_t to = static_cast<size_t>(edges[e].to);
            rank[to] = std::max(rank[to], rank[v] + edges[e].min_length);
            if (--pending[to] == 0) order.push_back(to);
        }
    }

    // A source with targets sits right above its nearest target, not at the top.
    for (size_t i = order.size(); i-- > 0;) {
        const size_t v = order[i];
        if (in_degree[v] != 0 || out_degree[v] == 0) continue;
        int limit = rank[static_cast<size_t>(edges[out[v][0]].to)] - edges[out[v][0]].min_length;
        for (size_t e : out[v])
            limit = std::min(limit, rank[static_cast<size_t>(edges[e].to)] - edges[e].min_length);
        rank[v] = std::max(rank[v], limit);
    }
    return rank;
}

// ------------------------------------------------------------ CrossingMinimizer

namespace {

std::vector<int> group_by_cluster(const std::vector<Vertex>& vs, const std::vector<int>& items,
                                  size_t depth) {
    std::vector<int> keys;
    std::map<int, std::vector<int>> buckets;
    for (int v : items) {
        const std::vector<int>& path = vs[static_cast<size_t>(v)].clusters;
        const int key = depth < path.size() ? path[depth] : -(v + 1);
        if (!buckets.count(key)) keys.push_back(key);
        buckets[key].push_back(v);
    }
    std::vector<int> out;
    for (int key : keys) {
        if (key < 0) {
            out.push_back(buckets[key].front());
        } else {
            const std::vector<int> inner = group_by_cluster(vs, buckets[key], depth + 1);
            out.insert(out.end(), inner.begin(), inner.end());
        }
    }
    return out;
}

}  // namespace

void CrossingMinimizer::run(LayeredGraph& g) const {
    std::vector<int> position(g.vertices.size(), 0);
    auto refresh = [&](const std::vector<int>& layer) {
        for (size_t i = 0; i < layer.size(); ++i) position[static_cast<size_t>(layer[i])] = static_cast<int>(i);
    };
    for (const auto& layer : g.layers) refresh(layer);

    auto sweep = [&](size_t index, const std::vector<std::vector<int>>& neighbours) {
        std::vector<int>& layer = g.layers[index];
        std::vector<double> weight(g.vertices.size(), 0.0);
        for (int v : layer) {
            const std::vector<int>& ns = neighbours[static_cast<size_t>(v)];
            double sum = 0;
            for (int n : ns) sum += position[static_cast<size_t>(n)];
            weight[static_cast<size_t>(v)] = ns.empty() ? position[static_cast<size_t>(v)] : sum / static_cast<double>(ns.size());
        }
        std::stable_sort(layer.begin(), layer.end(), [&](int a, int b) {
            return weight[static_cast<size_t>(a)] < weight[static_cast<size_t>(b)];
        });
        layer = group_by_cluster(g.vertices, layer, 0);
        refresh(layer);
    };

    for (int round = 0; round < 8; ++round) {
        for (size_t l = 1; l < g.layers.size(); ++l) sweep(l, g.up);
        for (size_t l = g.layers.size(); l-- > 1;) sweep(l - 1, g.down);
    }
}

// ----------------------------------------------------------- CoordinateAssigner

namespace {

size_t shared_prefix(const std::vector<int>& a, const std::vector<int>& b) {
    size_t n = 0;
    while (n < a.size() && n < b.size() && a[n] == b[n]) ++n;
    return n;
}

// Least squares with x[i+1] - x[i] >= gap[i]: pool adjacent violators.
std::vector<double> fit_in_order(const std::vector<double>& desired, const std::vector<double>& gap) {
    const size_t n = desired.size();
    std::vector<double> offset(n, 0.0), target(n);
    for (size_t i = 1; i < n; ++i) offset[i] = offset[i - 1] + gap[i - 1];
    for (size_t i = 0; i < n; ++i) target[i] = desired[i] - offset[i];

    struct Block {
        double mean;
        size_t count;
    };
    std::vector<Block> blocks;
    for (size_t i = 0; i < n; ++i) {
        blocks.push_back({target[i], 1});
        while (blocks.size() > 1 && blocks[blocks.size() - 2].mean > blocks.back().mean) {
            const Block b = blocks.back();
            blocks.pop_back();
            Block& a = blocks.back();
            a.mean = (a.mean * static_cast<double>(a.count) + b.mean * static_cast<double>(b.count)) /
                     static_cast<double>(a.count + b.count);
            a.count += b.count;
        }
    }
    std::vector<double> x;
    for (const Block& b : blocks)
        for (size_t k = 0; k < b.count; ++k) x.push_back(b.mean + offset[x.size()]);
    return x;
}

}  // namespace

double CoordinateAssigner::separation(const Vertex& a, const Vertex& b) const {
    const double half_a = (a.is_dummy() ? spacing_.edge_sep : spacing_.node_sep) / 2;
    const double half_b = (b.is_dummy() ? spacing_.edge_sep : spacing_.node_sep) / 2;
    const size_t shared = shared_prefix(a.clusters, b.clusters);
    const double boundaries = static_cast<double>(a.clusters.size() + b.clusters.size() - 2 * shared);
    return a.size.width / 2 + a.right_room + b.size.width / 2 + half_a + half_b +
           spacing_.cluster_pad * boundaries;
}

void CoordinateAssigner::assign(LayeredGraph& g) const {
    // y: stack the layers.
    double top = 0;
    for (size_t l = 0; l < g.layers.size(); ++l) {
        double height = 0;
        for (int v : g.layers[l]) height = std::max(height, g.vertices[static_cast<size_t>(v)].size.height);
        for (int v : g.layers[l]) g.vertices[static_cast<size_t>(v)].y = top + height / 2;
        const double extra = l < spacing_.extra_gap.size() ? spacing_.extra_gap[l] : 0.0;
        top += height + spacing_.rank_sep / 2 + extra;
    }

    // x: pack, then relax towards the neighbours. The last pass runs bottom-up so
    // long edges line up with their targets.
    for (const auto& layer : g.layers) {
        double x = 0;
        for (size_t i = 0; i < layer.size(); ++i) {
            if (i > 0) x += separation(g.vertices[static_cast<size_t>(layer[i - 1])], g.vertices[static_cast<size_t>(layer[i])]);
            g.vertices[static_cast<size_t>(layer[i])].x = x;
        }
    }

    auto relax = [&](size_t index, const std::vector<std::vector<int>>& neighbours) {
        const std::vector<int>& layer = g.layers[index];
        std::vector<double> desired(layer.size()), gap(layer.empty() ? 0 : layer.size() - 1);
        for (size_t i = 0; i < layer.size(); ++i) {
            const Vertex& v = g.vertices[static_cast<size_t>(layer[i])];
            const std::vector<int>& ns = neighbours[static_cast<size_t>(layer[i])];
            double sum = 0;
            for (int n : ns) sum += g.vertices[static_cast<size_t>(n)].x;
            desired[i] = ns.empty() ? v.x : sum / static_cast<double>(ns.size());
            if (i > 0) gap[i - 1] = separation(g.vertices[static_cast<size_t>(layer[i - 1])], v);
        }
        const std::vector<double> x = fit_in_order(desired, gap);
        for (size_t i = 0; i < layer.size(); ++i) g.vertices[static_cast<size_t>(layer[i])].x = x[i];
    };

    for (int round = 0; round < 8; ++round) {
        for (size_t l = 1; l < g.layers.size(); ++l) relax(l, g.up);
        for (size_t l = g.layers.size(); l-- > 1;) relax(l - 1, g.down);
    }
}

// ------------------------------------------------------------------ EdgeRouter

namespace {

struct PortKey {
    size_t chain;
    double neighbour_x;
};

void spread(std::vector<PortKey>& keys, const Vertex& v, std::map<size_t, double>& out) {
    std::sort(keys.begin(), keys.end(), [](const PortKey& a, const PortKey& b) {
        return a.neighbour_x != b.neighbour_x ? a.neighbour_x < b.neighbour_x : a.chain < b.chain;
    });
    const double span = std::max(0.0, v.size.width - 20);
    const double n = static_cast<double>(keys.size());
    for (size_t i = 0; i < keys.size(); ++i)
        out[keys[i].chain] = v.x - span / 2 + span * (static_cast<double>(i) + 1) / (n + 1);
}

void push(std::vector<Point>& pts, Point p) {
    if (!pts.empty() && pts.back() == p) return;
    pts.push_back(p);
}

std::vector<Point> without_collinear(const std::vector<Point>& pts) {
    std::vector<Point> out;
    for (const Point& p : pts) {
        while (out.size() >= 2) {
            const Point& a = out[out.size() - 2];
            const Point& b = out.back();
            const bool vertical = a.x == b.x && b.x == p.x;
            const bool horizontal = a.y == b.y && b.y == p.y;
            if (!vertical && !horizontal) break;
            out.pop_back();
        }
        out.push_back(p);
    }
    return out;
}

}  // namespace

std::vector<std::vector<Point>> EdgeRouter::route(const LayeredGraph& g) const {
    std::vector<std::map<size_t, double>> out_port(g.vertices.size()), in_port(g.vertices.size());
    std::vector<std::vector<PortKey>> outs(g.vertices.size()), ins(g.vertices.size());
    for (size_t c = 0; c < g.chains.size(); ++c) {
        const std::vector<int>& vs = g.chains[c].vertices;
        const size_t first = static_cast<size_t>(vs.front()), last = static_cast<size_t>(vs.back());
        outs[first].push_back({c, g.vertices[static_cast<size_t>(vs[1])].x});
        ins[last].push_back({c, g.vertices[static_cast<size_t>(vs[vs.size() - 2])].x});
    }
    for (size_t v = 0; v < g.vertices.size(); ++v) {
        spread(outs[v], g.vertices[v], out_port[v]);
        spread(ins[v], g.vertices[v], in_port[v]);
    }

    std::vector<std::vector<Point>> routes;
    for (size_t c = 0; c < g.chains.size(); ++c) {
        const std::vector<int>& vs = g.chains[c].vertices;
        const Vertex& src = g.vertices[static_cast<size_t>(vs.front())];
        std::vector<Point> pts;
        Point from{out_port[static_cast<size_t>(vs.front())][c], src.bottom()};
        push(pts, from);
        for (size_t i = 1; i < vs.size(); ++i) {
            const Vertex& v = g.vertices[static_cast<size_t>(vs[i])];
            const bool last = i + 1 == vs.size();
            const Point entry{last ? in_port[static_cast<size_t>(vs[i])][c] : v.x, v.top()};
            if (from.x != entry.x) {
                double bend = entry.y;  // jog on the top of the next corridor
                if (last) bend = (i == 1) ? (from.y + entry.y) / 2 : entry.y - rank_sep_ / 2;
                push(pts, {from.x, bend});
                push(pts, {entry.x, bend});
            }
            push(pts, entry);
            from = {entry.x, v.bottom()};
            if (!last) push(pts, from);
        }
        pts = without_collinear(pts);
        if (g.chains[c].reversed) std::reverse(pts.begin(), pts.end());
        routes.push_back(std::move(pts));
    }
    return routes;
}

}  // namespace mermaid::flowchart::detail

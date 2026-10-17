// dagre_order.h (private) - orders the nodes inside every rank to reduce edge crossings.
//
// Layer by layer sweeps (down, then up, ...) sort each rank by the barycenter of the
// positions of its neighbours in the previous rank, keeping the members of a compound
// node together. The ordering with the fewest crossings seen wins.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dagre_util.h"

namespace mermaid::dagre::detail {

// A node (or a whole sorted subgraph) with the average position of what it connects to.
struct Barycenter {
    std::string v;
    std::optional<double> value;
    double weight = 0;
};

// A group of nodes that are placed together, in this order.
struct SortEntry {
    std::vector<std::string> vs;
    int i = 0;  // position before sorting
    std::optional<double> barycenter;
    double weight = 0;
};

struct SortResult {
    std::vector<std::string> vs;
    std::optional<double> barycenter;
    double weight = 0;
};

// Constraint graph: an edge a -> b says subgraph a must stay left of subgraph b.
using ConstraintGraph = LayoutGraph;

class Orderer {
public:
    explicit Orderer(LayoutGraph& g, IdGenerator& ids) : g_(g), ids_(ids) {}
    void run();

private:
    enum class Direction { In, Out };

    LayoutGraph build_layer_graph(int rank, Direction direction);
    std::vector<LayoutGraph> build_layer_graphs(const std::vector<int>& ranks, Direction direction);
    void sweep(std::vector<LayoutGraph>& layer_graphs, bool bias_right);
    Layers initial_order() const;
    void assign_order(const Layers& layering);

    LayoutGraph& g_;
    IdGenerator& ids_;
};

// Pieces of the sweep, public inside `detail` so they can be named in this header only.
std::vector<Barycenter> barycenters(const LayoutGraph& g, const std::vector<std::string>& movable);
std::vector<SortEntry> resolve_conflicts(const std::vector<Barycenter>& entries, const ConstraintGraph& cg);
SortResult sort_entries(std::vector<SortEntry> entries, bool bias_right);
SortResult sort_subgraph(const LayoutGraph& g, const std::string& v, const ConstraintGraph& cg, bool bias_right);
void add_subgraph_constraints(const LayoutGraph& g, ConstraintGraph& cg, const std::vector<std::string>& vs);
double cross_count(const LayoutGraph& g, const Layers& layering);

}  // namespace mermaid::dagre::detail

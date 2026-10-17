// dagre_rank.h (private) - assigns every node a rank (layer) with the network simplex method.
#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "dagre_util.h"

namespace mermaid::dagre::detail {

// Gansner et al., "A technique for drawing directed graphs": a feasible spanning tree is
// improved by exchanging tree edges until no cut value is negative. Ranks are written to
// the node labels of the graph that is passed in.
class NetworkSimplexRanker {
public:
    explicit NetworkSimplexRanker(const LayoutGraph& g) : g_(simplify(g)) {}

    void run();

private:
    using Tree = LayoutGraph;  // undirected; its node labels carry low / lim / parent

    void longest_path();
    void build_feasible_tree();
    size_t grow_tight_tree();
    EdgeObj find_min_slack_edge() const;
    double slack(const EdgeObj& e) const;

    void init_low_lim_values();
    int assign_low_lim(const std::string& v, const std::string& parent, int next_lim,
                       std::unordered_set<std::string>& visited);
    void init_cut_values();
    double calc_cut_value(const std::string& child) const;
    bool find_leaving_edge(EdgeObj& out) const;
    EdgeObj find_entering_edge(const EdgeObj& leaving) const;
    void exchange_edges(const EdgeObj& leaving, const EdgeObj& entering);
    void update_ranks();
    bool is_descendant(const NodeLabel& v, const NodeLabel& root) const;

    std::vector<std::string> postorder(const std::string& root) const;
    std::vector<std::string> preorder(const std::string& root) const;

    LayoutGraph g_;                // simplified copy; node labels shared with the caller's graph
    std::unique_ptr<Tree> tree_;
};

}  // namespace mermaid::dagre::detail

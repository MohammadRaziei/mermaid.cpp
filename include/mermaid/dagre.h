// dagre.h - layered graph layout (the algorithm Mermaid lays flowcharts out with).
//
// Input is a directed multigraph with optional nesting (compound nodes). Nodes have a
// size; edges may carry a label size. The result gives every node a centre, every edge
// a polyline and every labelled edge a label position. Behaviour follows dagre-d3-es
// (network-simplex ranking, barycenter ordering, Brandes-Koepf positioning).
#pragma once

#include <string>
#include <vector>

#include <mermaid/geometry.h>

namespace mermaid::dagre {

enum class RankDir { TB, BT, LR, RL };
enum class LabelPos { Left, Center, Right };

struct Options {
    RankDir rankdir = RankDir::TB;
    double nodesep = 50;  // gap between nodes of one rank
    double edgesep = 20;  // gap around edge segments
    double ranksep = 50;  // gap between ranks
    double marginx = 0;
    double marginy = 0;
};

struct Node {
    std::string id;
    double width = 0;
    double height = 0;
    std::string parent;  // id of the enclosing compound node; empty = top level
};

struct Edge {
    std::string v;     // source
    std::string w;     // target
    std::string name;  // tells parallel edges apart; empty = unnamed
    double minlen = 1;
    double weight = 1;
    double width = 0;  // label size; zero = no label
    double height = 0;
    double labeloffset = 10;
    LabelPos labelpos = LabelPos::Right;
};

struct Input {
    Options options;
    std::vector<Node> nodes;  // a node's parent must be listed too
    std::vector<Edge> edges;  // both ends must be listed
};

struct NodeResult {
    std::string id;
    double x = 0;  // centre
    double y = 0;
    double width = 0;  // compound nodes: the computed size
    double height = 0;
};

struct EdgeResult {
    std::string v;
    std::string w;
    std::string name;
    std::vector<Point> points;  // from the source border to the target border
    bool has_label = false;
    Point label;  // centre; valid when has_label
    double label_width = 0;
    double label_height = 0;
};

struct Result {
    std::vector<NodeResult> nodes;  // input order
    std::vector<EdgeResult> edges;  // input order
    double width = 0;
    double height = 0;

    const NodeResult* find_node(const std::string& id) const;
    const EdgeResult* find_edge(const std::string& v, const std::string& w,
                                const std::string& name = {}) const;
};

// Throws std::invalid_argument for unknown ids or a parent cycle.
Result layout(const Input& input);

}  // namespace mermaid::dagre

// dagre_normalize.h (private) - the stages that reshape the graph around its ranks:
// long edges become chains of dummy nodes, compound nodes get border columns.
#pragma once

#include "dagre_util.h"

namespace mermaid::dagre::detail {

// Splits every edge that spans more than one rank into a chain of one-rank edges
// through dummy nodes; `undo` removes the chains and records their positions as the
// edge's points.
class EdgeNormalizer {
public:
    EdgeNormalizer(LayoutGraph& g, IdGenerator& ids) : g_(g), ids_(ids) {}

    void run();
    void undo();

private:
    void normalize_edge(const EdgeObj& e);

    LayoutGraph& g_;
    IdGenerator& ids_;
};

// Dummy nodes of a chain belong to the compound node that contains the edge's
// path at that rank (the lowest common ancestor and its descendants on the way).
class ParentDummyChains {
public:
    explicit ParentDummyChains(LayoutGraph& g) : g_(g) {}
    void run();

private:
    LayoutGraph& g_;
};

// One left and one right border node per rank for every compound node.
class BorderSegments {
public:
    BorderSegments(LayoutGraph& g, IdGenerator& ids) : g_(g), ids_(ids) {}
    void run();

private:
    void visit(const std::string& v);
    void add_border(bool left, const std::string& compound, NodeLabel& compound_label, int rank);

    LayoutGraph& g_;
    IdGenerator& ids_;
};

}  // namespace mermaid::dagre::detail

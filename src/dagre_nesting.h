// dagre_nesting.h (private) - lets ranking handle nested (compound) nodes.
//
// A root node and top/bottom border nodes per compound node are added, with heavy
// "nesting" edges that keep every child between the borders of its parent.
#pragma once

#include "dagre_util.h"

namespace mermaid::dagre::detail {

class NestingGraph {
public:
    NestingGraph(LayoutGraph& g, IdGenerator& ids) : g_(g), ids_(ids) {}

    void run();
    void cleanup();

private:
    void connect(const std::string& root, double node_sep, double weight, int height,
                 const std::map<std::string, int>& depths, const std::string& v);

    LayoutGraph& g_;
    IdGenerator& ids_;
};

}  // namespace mermaid::dagre::detail

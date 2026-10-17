// dagre_acyclic.h (private) - makes the graph acyclic by reversing back edges.
#pragma once

#include "dagre_util.h"

namespace mermaid::dagre::detail {

class Acyclic {
public:
    Acyclic(LayoutGraph& g, IdGenerator& ids) : g_(g), ids_(ids) {}

    // Depth-first search; every edge that closes a cycle is reversed.
    void run();
    // Puts the reversed edges back the way they were.
    void undo();

private:
    LayoutGraph& g_;
    IdGenerator& ids_;
};

}  // namespace mermaid::dagre::detail

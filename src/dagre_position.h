// dagre_position.h (private) - gives every node its coordinates.
//
// Y comes from the ranks (the tallest node of a rank sets its height). X is computed with
// the Brandes-Koepf algorithm: four vertical alignments (up/down, left/right) are made
// compact and then balanced.
#pragma once

#include "dagre_util.h"

namespace mermaid::dagre::detail {

// Sets x and y on every node that has no children (compound nodes are sized later).
void position(LayoutGraph& g);

}  // namespace mermaid::dagre::detail

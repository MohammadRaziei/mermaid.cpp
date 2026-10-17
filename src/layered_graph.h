// layered_graph.h (private) - the graph the layout steps work on.
//
// Coordinates here are in *flow orientation*: layers run along y and the order
// inside a layer runs along x. LayoutEngine turns that into the real direction.
#pragma once

#include <vector>

#include <mermaid/geometry.h>

namespace mermaid::flowchart::detail {

enum class VertexKind { Node, Waypoint, Label };

struct Vertex {
    VertexKind kind = VertexKind::Node;
    int node = -1;             // model node index (Node)
    int edge = -1;             // model edge index (Waypoint, Label)
    Size size;                 // width = cross axis, height = along the flow
    int layer = 0;
    double x = 0;              // centre
    double y = 0;
    double right_room = 0;     // extra space wanted to the right (self loops)
    std::vector<int> clusters; // enclosing subgraphs, outermost first

    bool is_dummy() const { return kind != VertexKind::Node; }
    double top() const { return y - size.height / 2; }
    double bottom() const { return y + size.height / 2; }
};

// An edge as a path of vertices, lowest layer first. `reversed` means the model
// edge points from the last vertex to the first (it was flipped to break a cycle).
struct Chain {
    int edge = -1;
    bool reversed = false;
    std::vector<int> vertices;
};

struct LayeredGraph {
    std::vector<Vertex> vertices;
    std::vector<Chain> chains;
    std::vector<std::vector<int>> layers;  // vertex ids, in order
    std::vector<std::vector<int>> up;      // neighbours in the previous layer
    std::vector<std::vector<int>> down;    // neighbours in the next layer

    void build_layers();     // fills layers from Vertex::layer (creation order)
    void build_adjacency();  // fills up / down from the chains
};

}  // namespace mermaid::flowchart::detail

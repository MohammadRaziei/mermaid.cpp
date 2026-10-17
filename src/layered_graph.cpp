#include "layered_graph.h"

#include <algorithm>

namespace mermaid::flowchart::detail {

void LayeredGraph::build_layers() {
    int count = 0;
    for (const Vertex& v : vertices) count = std::max(count, v.layer + 1);
    layers.assign(static_cast<size_t>(count), {});
    for (size_t i = 0; i < vertices.size(); ++i)
        layers[static_cast<size_t>(vertices[i].layer)].push_back(static_cast<int>(i));
}

void LayeredGraph::build_adjacency() {
    up.assign(vertices.size(), {});
    down.assign(vertices.size(), {});
    for (const Chain& chain : chains) {
        for (size_t i = 0; i + 1 < chain.vertices.size(); ++i) {
            const int a = chain.vertices[i];
            const int b = chain.vertices[i + 1];
            down[static_cast<size_t>(a)].push_back(b);
            up[static_cast<size_t>(b)].push_back(a);
        }
    }
}

}  // namespace mermaid::flowchart::detail

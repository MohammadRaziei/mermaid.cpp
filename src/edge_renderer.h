// edge_renderer.h (private) - draws edges: paths, arrow heads and labels (Mermaid 12.2 neo look).
#pragma once

#include <map>
#include <string>
#include <vector>

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>

#include "svg_writer.h"

namespace mermaid::detail {

class EdgeRenderer {
public:
    EdgeRenderer(std::string svg_id, double line_height)
        : svg_id_(std::move(svg_id)), line_height_(line_height) {}

    // The 12 <marker> elements; goes inside the marker group, before <g class="root">.
    void write_markers(SvgWriter& out) const;

    // The coloured marker copies that edges with a `stroke:` style asked for while their paths
    // were written; Mermaid appends them after the root group.
    void write_colored_markers(SvgWriter& out) const;

    // Invisible edges and edges without a route draw nothing. `tail` and `head` are the layouts
    // of the end nodes (null for a subgraph): the route is clipped to their outlines.
    // An end at a subgraph id is a cluster: `tail`/`head` is then the cluster box as a plain rectangle, and the
    // route is cut at its border (edges.js `toCluster` / `fromCluster`).
    struct Ends {
        const flowchart::NodeLayout* tail = nullptr;
        const flowchart::NodeLayout* head = nullptr;
        bool tail_is_cluster = false;
        bool head_is_cluster = false;
    };
    void write_path(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout, const Ends& ends);
    void write_label(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) const;

    static bool is_drawn(const flowchart::EdgeLayout& layout);

private:
    struct colored_marker {
        std::string id;      // id of the copy
        std::string source;  // marker name without the "<svg>_flowchart-v2-" prefix
        std::string color;
        bool filled = false;
    };

    std::string marker_url(flowchart::Head head, bool start, const std::string& style_text);
    void write_marker(SvgWriter& out, const std::string& name, const std::string& id, const std::string& color, bool filled) const;

    std::string svg_id_;
    double line_height_;
    std::vector<colored_marker> colored_;
    // positionEdgeLabel: how far the drawn path moved the middle of the route, by edge id (set by write_path)
    std::map<std::string, Point> label_shifts_;
};

}  // namespace mermaid::detail

// edge_renderer.h (private) - draws edges: paths, arrow heads and labels.
#pragma once

#include <string>

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>

#include "svg_writer.h"
#include "theme.h"

namespace mermaid::detail {

class EdgeRenderer {
public:
    EdgeRenderer(const Theme& theme, std::string svg_id, double line_height)
        : theme_(theme), svg_id_(std::move(svg_id)), line_height_(line_height) {}

    // The <marker> elements; goes inside <defs>.
    void write_markers(SvgWriter& out) const;

    // Invisible edges and edges without a route draw nothing.
    void write_path(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) const;
    void write_label(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) const;

    static bool is_drawn(const flowchart::Edge& edge, const flowchart::EdgeLayout& layout);

private:
    std::string marker_id(flowchart::Head head, bool start) const;

    const Theme& theme_;
    std::string svg_id_;
    double line_height_;
};

}  // namespace mermaid::detail

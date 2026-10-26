// node_renderer.h (private) - draws one flowchart node the way Mermaid's neo look does:
// a group translated to the node centre, the outline in local coordinates, and an HTML label
// (<foreignObject>) centred in it.
#pragma once

#include <string>
#include <vector>

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>

#include "rough_path.h"
#include "shape_geometry.h"
#include "svg_writer.h"
#include "theme_variables.h"

namespace mermaid::detail {

class node_renderer {
public:
    node_renderer(std::string svg_id, theme_variables theme, int hand_drawn_seed)
        : svg_id_(std::move(svg_id)), theme_(std::move(theme)), seed_(hand_drawn_seed) {}

    // `style` is the node's resolved style (classDef, then `style` statements).
    void write(SvgWriter& out, const flowchart::Node& node, const flowchart::NodeLayout& layout,
               const flowchart::Style& style) const;

private:
    void write_outline(SvgWriter& out, flowchart::Shape shape, const Size& size, const Size& label, const flowchart::Style& style) const;
    void write_label(SvgWriter& out, const std::string& text, const Size& label, const flowchart::Style& style,
                     const Point& shift, bool markdown, bool table, bool wrapped, bool tight = false) const;
    void write_fork(SvgWriter& out, const Rect& box, const flowchart::Style& style, const std::string& inline_style) const;
    void write_empty_label(SvgWriter& out, const flowchart::Style& style, bool markdown) const;
    void write_rough_paths(SvgWriter& out, const rough_drawing& drawing, const flowchart::Style& style,
                           const std::string& inline_style, bool polygon_fill, bool stroke_only = false) const;
    void write_circle_shape(SvgWriter& out, flowchart::Shape shape, const flowchart::Style& style, const std::string& inline_style) const;
    void write_merged_path(SvgWriter& out, const rough_drawing& drawing, const flowchart::Style& style,
                           const std::string& inline_style) const;
    void write_rough_layers(SvgWriter& out, const polygon_outline& outline, const flowchart::Style& style,
                            const std::string& inline_style) const;
    void write_rough_polygon(SvgWriter& out, const std::vector<Point>& points, const std::string& transform,
                             const flowchart::Style& style, const std::string& inline_style,
                             const char* group_class = "basic label-container outer-path") const;
    void write_stadium(SvgWriter& out, const Size& size, const flowchart::Style& style, const std::string& inline_style) const;
    static double label_shift(flowchart::Shape shape);

    std::string svg_id_;
    theme_variables theme_;
    int seed_;
};

}  // namespace mermaid::detail

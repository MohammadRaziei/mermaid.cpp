// node_renderer.h (private) - draws one flowchart node the way Mermaid's neo look does:
// a group translated to the node centre, the outline in local coordinates, and an HTML label
// (<foreignObject>) centred in it.
#pragma once

#include <string>

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>

#include "svg_writer.h"

namespace mermaid::detail {

class node_renderer {
public:
    explicit node_renderer(std::string svg_id) : svg_id_(std::move(svg_id)) {}

    // `style` is the node's resolved style (classDef, then `style` statements).
    void write(SvgWriter& out, const flowchart::Node& node, const flowchart::NodeLayout& layout,
               const flowchart::Style& style) const;

private:
    void write_outline(SvgWriter& out, flowchart::Shape shape, const Size& size, const flowchart::Style& style) const;
    void write_label(SvgWriter& out, const std::string& text, const Size& label, const flowchart::Style& style) const;

    std::string svg_id_;
};

}  // namespace mermaid::detail

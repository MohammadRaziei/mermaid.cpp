// shape_renderer.h (private) - draws the outline of a node.
#pragma once

#include <string>

#include <mermaid/flowchart.h>
#include <mermaid/geometry.h>

#include "svg_writer.h"

namespace mermaid::detail {

// One subclass per kind of outline (box, circle, polygon, cylinder, subroutine).
class ShapeRenderer {
public:
    virtual ~ShapeRenderer() = default;

    // Writes the outline of `box`. `style` goes into the style attribute when not empty.
    virtual void draw(SvgWriter& out, const Rect& box, const std::string& style) const = 0;

    static const ShapeRenderer& for_shape(flowchart::Shape shape);
};

}  // namespace mermaid::detail

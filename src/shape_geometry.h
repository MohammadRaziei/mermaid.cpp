// shape_geometry.h (private) - what the neo look's shapes (shapes/*.ts) say about their outline, derived from the
// size of the node, so the sizer, the renderer and the edge clipping agree.
#pragma once

#include <optional>
#include <vector>

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>
#include <mermaid/geometry.h>

namespace mermaid::detail {

// `ignoreMinWidth` of labelHelper: circles size their label to the text, other shapes to at least 120.
bool ignores_min_label_width(flowchart::Shape shape);

// A shape drawn as a polygon (hexagon, lean, trapezoid, subroutine) or a rough polygon (odd).
struct polygon_outline {
    std::vector<Point> points;  // as the shape computes them, in its own local coordinates
    Size anchor;                // (w, h) of insertPolygonShape: the polygon is moved by translate(-w / 2, h / 2)
    bool rough = false;         // drawn by rough.js inside a group (odd), not as a <polygon>
    Point shift;                // transform of that group
};

// nullopt for the shapes that are not polygons. `size` is the node's size, the bounding box of the outline.
std::optional<polygon_outline> polygon_outline_of(flowchart::Shape shape, const Size& size);

// The label sits at the centre of the node, except that odd moves it with its notch.
Point label_offset(flowchart::Shape shape, const Size& size);

// intersect-polygon.js: where the line from the node centre to `outside` leaves the outline. nullopt when the shape
// is not a polygon.
std::optional<Point> polygon_border_point(const flowchart::NodeLayout& node, const Point& outside);

}  // namespace mermaid::detail

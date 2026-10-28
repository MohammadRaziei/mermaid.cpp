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

// Shapes drawn as one path of elliptical arcs (cloud, bang): the path data as Mermaid writes it, the size of its
// bounding box and the box the path is laid out in (`nominal`: the transform is translate(-w / 2, -h / 2)).
struct arc_shape {
    std::string d;
    Size bounds;
    Size nominal;
    Point origin;  // top left of the bounding box, from where the path starts
};
std::optional<arc_shape> arc_shape_of(flowchart::Shape shape, const Size& label);

// True for the shapes whose label transform is written by the shape itself, as `translate(x,y)`.
bool writes_tight_label_transform(flowchart::Shape shape);

// A shape drawn as a polygon (hexagon, lean, trapezoid, subroutine) or a rough polygon (odd).
struct polygon_outline {
    std::vector<Point> points;  // as the shape computes them, in its own local coordinates
    Size anchor;                // (w, h) of insertPolygonShape: the polygon is moved by translate(-w / 2, h / 2)
    bool rough = false;         // drawn by rough.js inside a group (odd), not as a <polygon>
    Point shift;                // transform of that group
    const char* separator = ",";  // between the numbers of that transform (shapes differ: "5.6,0" or "-15, -15")
    const char* group_class = "basic label-container outer-path";  // class of that group
    // Shapes made of several rough.js drawings (documents): in document order, a `nested` one sits in a <g> of its own.
    // `points` is then only what edges are clipped to.
    struct layer {
        std::vector<Point> points;
        bool nested = false;
        bool polygon_fill = false;  // rc.polygon: the fill is a plain `M L L`, tagged fill-rule="evenodd"
        bool merged = false;        // mergePaths: fill and outline become one <path>
        bool stroke_only = false;   // rc.path with fill "none": no fill <path>
        bool open = false;          // the path text had its `Z` removed
        bool hidden = false;        // the <g> of the drawing has stroke-opacity 0
        std::vector<std::vector<Point>> more;  // further open runs of the same path (their outline only), after `points`
    };
    std::vector<layer> layers;
    bool hit_as_rect = false;  // edges are clipped to the node box (intersect.rect), not to the outline
    bool group_style = false;  // the group carries an (empty) style attribute: shadedProcess.ts sets `style` from cssStyles
    bool has_shift = true;  // the group carries translate(shift); flag does not
    bool plain_path = false;  // one `<path d="M L L Z">` in node coordinates, no transform (folder)
};

// bucket.ts: where the cup is drawn, in node coordinates (the drawing is not centred on the node box).
struct bucket_geometry {
    double w, bottom_w, rim_ry, top_y, bottom_y;
};
bucket_geometry bucket_geometry_of(const Size& size);

// person.ts: the head and the body, in node coordinates (padding 15 instead of the shape's own 20).
struct person_geometry {
    double w, head_radius, overlap, body_height, body_radius, total, top, body_top;
};
person_geometry person_geometry_of(const Size& label);

// How far a shape's drawing is moved from the centre of its node box (bucket only).
Point drawing_shift(flowchart::Shape shape, const Size& size);

// nullopt for the shapes that are not polygons. `size` is the node's size, the bounding box of the outline.
// `label` is the label box, from which the document shapes derive their outline exactly.
std::optional<polygon_outline> polygon_outline_of(flowchart::Shape shape, const Size& size, const Size& label = {});

// The label sits at the centre of the node, except that odd moves it with its notch.
Point label_offset(flowchart::Shape shape, const Size& size, const Size& label = {});

// intersect-polygon.js: where the line from the node centre to `outside` leaves the outline. nullopt when the shape
// is not a polygon.
std::optional<Point> polygon_border_point(const flowchart::NodeLayout& node, const Point& outside);

}  // namespace mermaid::detail

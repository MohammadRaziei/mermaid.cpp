// edge_geometry.h - the geometry Mermaid applies to a laid out edge before drawing it:
// clipping the route to the node outlines, shortening it for the arrow heads, rounding its
// corners, and measuring the result.
#pragma once

#include <string>
#include <vector>

#include <mermaid/flowchart_layout.h>
#include <mermaid/geometry.h>

namespace mermaid {

class edge_geometry {
public:
    // Mermaid drops the first and last layout point and replaces them by the points where the
    // lines to the neighbouring points cross the node outlines (`node.intersect`). A route with
    // no inner point is clipped along the line between the two node centres.
    static std::vector<Point> clip_to_nodes(const std::vector<Point>& route, const flowchart::NodeLayout& tail,
                                            const flowchart::NodeLayout& head);

    // Where the line from the node centre towards `outside` leaves the node outline.
    static Point border_point(const flowchart::NodeLayout& node, const Point& outside);

    // `applyMarkerOffsetsToPoints`: moves the ends towards the inside by the given distances.
    static std::vector<Point> shorten_ends(const std::vector<Point>& points, double start, double end);

    // `generateRoundedPath`: straight lines with quadratic corners of the given radius.
    static std::string rounded_path(const std::vector<Point>& points, double radius);

    // Length of the rounded path (what the browser reports for getTotalLength()).
    static double rounded_length(const std::vector<Point>& points, double radius);

    // `fixCorners` (edges.js): right-angle corners (both legs longer than 5) are cut 5 units from
    // the corner, and the corner itself is pulled in, before a curve is fitted through the route.
    static std::vector<Point> fix_corners(const std::vector<Point>& points);

    // The path d3 draws through the points for an edge `curve` name ("basis" - the default -,
    // "linear" or "cardinal"; any other name draws "basis"), written like d3-path with 3 digits.
    static std::string curve_path(const std::string& curve, const std::vector<Point>& points);

    // The tight bounding box of that path (what getBBox() reports): the curve itself, sampled numerically, not its
    // control points.
    static Rect curve_bounds(const std::string& curve, const std::vector<Point>& points);

    // How far the path is pulled back from the tip of an arrow head (markerOffsets): 4 for an arrow, else 0.
    static double path_offset(flowchart::Head head);

    // Length of that path (what getTotalLength() reports), measured numerically.
    // utils.calcLabelPosition: the point half way along the polyline (coordinates rounded to 5 decimals).
    static Point label_position(const std::vector<Point>& points);

    // utils.isLabelCoordinateInPath: whether the rounded x or y of `point` occurs as text in the path `d`, whose
    // decimals are rounded the same way. When it does not, Mermaid moves the edge label with the drawn path.
    static bool is_label_coordinate_in_path(const Point& point, const std::string& path);

    static double curve_length(const std::string& curve, const std::vector<Point>& points);

    // The `data-points` value: base64 of the JSON list of points.
    static std::string points_attribute(const std::vector<Point>& points);
};

}  // namespace mermaid

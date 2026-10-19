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

    // The `data-points` value: base64 of the JSON list of points.
    static std::string points_attribute(const std::vector<Point>& points);
};

}  // namespace mermaid

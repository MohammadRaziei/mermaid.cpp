// Expected values come from Mermaid 12.2's own SVG of fixture `basic` (tests/data/mermaid_js).
#include <mermaid/edge_geometry.h>

#include "utest.h"

using namespace mermaid;
using flowchart::NodeLayout;
using flowchart::Shape;

namespace {
NodeLayout node(Shape shape, double x, double y, double w, double h) { return NodeLayout{"n", shape, {x, y}, {w, h}, {w, h}}; }
}  // namespace

UTEST(edge_geometry, border_point_of_a_box_is_where_the_line_from_the_centre_leaves_it) {
    const Point p = edge_geometry::border_point(node(Shape::Default, 100, 50, 152, 45), {100, 200});
    ASSERT_NEAR(100.0, p.x, 1e-9);
    ASSERT_NEAR(72.5, p.y, 1e-9);
    const Point q = edge_geometry::border_point(node(Shape::Default, 100, 50, 152, 45), {400, 50});
    ASSERT_NEAR(176.0, q.x, 1e-9);
    ASSERT_NEAR(50.0, q.y, 1e-9);
}

UTEST(edge_geometry, border_point_of_a_diamond_and_a_circle) {
    // Straight below the diamond of basic.svg (centre 184,182.5, 171 wide) the outline is its bottom vertex.
    const Point d = edge_geometry::border_point(node(Shape::Diamond, 184, 182.5, 171, 171), {184, 400});
    ASSERT_NEAR(184.0, d.x, 1e-9);
    ASSERT_NEAR(268.0, d.y, 1e-9);
    // Towards a point on a slope the line from the centre meets the diamond edge x/w + y/h = 1.
    const Point e = edge_geometry::border_point(node(Shape::Diamond, 0, 0, 100, 100), {100, 100});
    ASSERT_NEAR(25.0, e.x, 1e-9);
    ASSERT_NEAR(25.0, e.y, 1e-9);
    const Point c = edge_geometry::border_point(node(Shape::Circle, 0, 0, 20, 20), {30, 0});
    ASSERT_NEAR(10.0, c.x, 1e-9);
}

UTEST(edge_geometry, clip_to_nodes_replaces_the_first_and_last_point) {
    const NodeLayout tail = node(Shape::Diamond, 184, 182.5, 171, 171), head = node(Shape::Default, 184, 391.5, 152, 45);
    const std::vector<Point> route{{184, 260}, {184, 318.5}, {184, 380}};
    const auto clipped = edge_geometry::clip_to_nodes(route, tail, head);
    ASSERT_EQ(3u, clipped.size());
    ASSERT_NEAR(268.0, clipped.front().y, 1e-9);  // bottom vertex of the diamond
    ASSERT_NEAR(318.5, clipped[1].y, 1e-9);
    ASSERT_NEAR(369.0, clipped.back().y, 1e-9);   // top of the box
    // A route without an inner point is clipped along the line between the centres.
    const auto straight = edge_geometry::clip_to_nodes({{0, 0}, {0, 1}}, tail, head);
    ASSERT_EQ(2u, straight.size());
    ASSERT_NEAR(268.0, straight.front().y, 1e-9);
}

UTEST(edge_geometry, shorten_ends_moves_the_last_point_back_along_the_last_segment) {
    const auto p = edge_geometry::shorten_ends({{184, 57}, {184, 97}}, 0, 4);
    ASSERT_NEAR(93.0, p.back().y, 1e-9);
    ASSERT_NEAR(57.0, p.front().y, 1e-9);
}

UTEST(edge_geometry, rounded_path_matches_mermaid_for_the_basic_edge) {
    const std::vector<Point> pts{{158.83333333333331, 242.83326625823975}, {158.83333333333331, 268}, {158.83333333333331, 288}, {88, 288}, {88, 365}};
    ASSERT_STREQ("M158.833,242.833L158.833,268L158.833,280.929Q158.833,288 151.762,288L95.071,288Q88,288 88,295.071L88,365",
                 edge_geometry::rounded_path(pts, 5).c_str());
    ASSERT_STREQ("M184,57L184,93", edge_geometry::rounded_path({{184, 57}, {184, 93}}, 5).c_str());
}

UTEST(edge_geometry, rounded_length_matches_the_browsers_total_length) {
    // The style of the edges in basic.svg: stroke-dasharray "0 0 <length - 4> 4".
    ASSERT_NEAR(36.0, edge_geometry::rounded_length({{184, 57}, {184, 93}}, 5), 1e-9);
    const std::vector<Point> pts{{158.83333333333331, 242.83326625823975}, {158.83333333333331, 268}, {158.83333333333331, 288}, {88, 288}, {88, 365}};
    ASSERT_NEAR(183.67178 + 4, edge_geometry::rounded_length(pts, 5), 0.05);
}

UTEST(edge_geometry, points_attribute_is_base64_json_as_in_mermaid) {
    ASSERT_STREQ("W3sieCI6MTg0LCJ5Ijo1N30seyJ4IjoxODQsInkiOjk3fV0=", edge_geometry::points_attribute({{184, 57}, {184, 97}}).c_str());
}

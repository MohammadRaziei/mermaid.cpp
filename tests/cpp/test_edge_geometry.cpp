// Expected values come from Mermaid 12.2's own SVG of fixture `basic` (tests/data/mermaid_js).
#include <mermaid/edge_geometry.h>

#include <cmath>

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

// ---- curveBasis (the default curve of dagre layouts); numbers from tests/data/mermaid_js_dagre/basic.svg ----

UTEST(edge_geometry, basis_path_of_a_straight_three_point_route) {
    const std::vector<Point> p{{185, 53}, {185, 78}, {185, 99}};
    ASSERT_STREQ("M185,53L185,57.167C185,61.333,185,69.667,185,77.333C185,85,185,92,185,95.5L185,99", edge_geometry::curve_path("basis", p).c_str());
}

UTEST(edge_geometry, basis_path_of_a_bent_route_and_its_length) {
    const std::vector<Point> p{{146.10135135135135, 235.10135135135135}, {84, 309.5}, {84, 341}};
    ASSERT_STREQ("M146.101,235.101L135.751,247.501C125.401,259.901,104.7,284.7,94.35,302.35C84,320,84,330.5,84,335.75L84,341",
                 edge_geometry::curve_path("basis", p).c_str());
    ASSERT_NEAR(121.61546325683594 + 4, edge_geometry::curve_length("basis", p), 0.01);  // dasharray 0 0 121.615 4
}

UTEST(edge_geometry, basis_path_of_two_points_is_a_line) {
    ASSERT_STREQ("M0,0L10,20", edge_geometry::curve_path("basis", {{0, 0}, {10, 20}}).c_str());
}

UTEST(edge_geometry, fix_corners_pre_rounds_right_angles_only) {
    // edges.js fixCorners: a right angle with both legs > 5 is cut 5 from the corner and the corner pulled in.
    const auto fixed = edge_geometry::fix_corners({{0, 0}, {0, 100}, {100, 100}});
    ASSERT_EQ(5u, fixed.size());
    ASSERT_NEAR(0.0, fixed[1].x - 0.0, 1e-9);
    ASSERT_NEAR(95.0, fixed[1].y, 1e-9);
    ASSERT_NEAR(5.0, fixed[3].x, 1e-9);
    ASSERT_NEAR(100.0, fixed[3].y, 1e-9);
    const auto same = edge_geometry::fix_corners({{0, 0}, {50, 50}, {100, 0}});
    ASSERT_EQ(3u, same.size());
}

// ---- other curves (`e2@{curve: cardinal}`); numbers from tests/data/mermaid_js_dagre/stress_flowchart_amp_groups_edge_ids_034.svg ----

UTEST(edge_geometry, cardinal_path_and_length) {
    const std::vector<Point> p{{232.8421052631579, 148}, {286, 173}, {286, 194}};
    ASSERT_STREQ("M232.842,148C232.842,148,277.14,165.333,286,173C294.86,180.667,286,194,286,194",
                 edge_geometry::curve_path("cardinal", p).c_str());
    ASSERT_NEAR(77.80570983886719 + 4, edge_geometry::curve_length("cardinal", p), 0.01);
}

UTEST(edge_geometry, linear_path_joins_the_points) {
    ASSERT_STREQ("M0,0L10,20L30,20", edge_geometry::curve_path("linear", {{0, 0}, {10, 20}, {30, 20}}).c_str());
    ASSERT_NEAR(std::sqrt(500.0) + 20.0, edge_geometry::curve_length("linear", {{0, 0}, {10, 20}, {30, 20}}), 1e-9);
}

// numbers from tests/data/mermaid_js_dagre/s14_curve_*.svg (first edge of each fixture)
UTEST(edge_geometry, step_family_path) {
    const std::vector<Point> p{{137.1578947368421, 53}, {84, 78}, {84, 99}};
    ASSERT_STREQ("M137.158,53L110.579,53L110.579,78L84,78L84,99L84,99", edge_geometry::curve_path("step", p).c_str());
    ASSERT_STREQ("M137.158,53L84,53L84,78L84,78L84,99", edge_geometry::curve_path("stepAfter", p).c_str());
    ASSERT_STREQ("M137.158,53L137.158,78L84,78L84,99L84,99", edge_geometry::curve_path("stepBefore", p).c_str());
}

UTEST(edge_geometry, monotone_x_path) {
    const std::vector<Point> p{{131.8421052631579, 55.5}, {185, 30.5}, {206, 30.5}};
    ASSERT_STREQ("M131.842,55.5C149.561,43,167.281,30.5,185,30.5C192,30.5,199,30.5,206,30.5",
                 edge_geometry::curve_path("monotoneX", p).c_str());
}

UTEST(edge_geometry, bump_and_natural_keep_their_end_points) {
    const std::vector<Point> p{{0, 0}, {10, 30}, {40, 40}};
    for (const char* name : {"bumpX", "bumpY", "natural", "catmullRom", "monotoneY"}) {
        const std::string d = edge_geometry::curve_path(name, p);
        ASSERT_TRUE(d.rfind("M0,0C", 0) == 0);
        ASSERT_TRUE(d.size() > 5 && d.compare(d.size() - 5, 5, "40,40") == 0);
    }
    ASSERT_STREQ("M0,0L10,30", edge_geometry::curve_path("natural", {{0, 0}, {10, 30}}).c_str());
}

UTEST(edge_geometry, unknown_curve_names_fall_back_to_basis) {
    const std::vector<Point> p{{0, 0}, {10, 30}, {40, 40}};
    ASSERT_STREQ(edge_geometry::curve_path("basis", p).c_str(), edge_geometry::curve_path("", p).c_str());
    ASSERT_STREQ(edge_geometry::curve_path("basis", p).c_str(), edge_geometry::curve_path("nonsense", p).c_str());
}

// intersectPolygon: the outline of a hexagon / lean / trapezoid / odd / subroutine node is its polygon, not its box.
UTEST(edge_geometry, border_point_of_a_hexagon_is_on_its_slanted_side) {
    // 204 x 91, corners at (+-102, 0), (+-76, +-45.5) around the centre
    const NodeLayout hexagon = node(Shape::Hexagon, 0, 0, 204, 91);
    const Point p = edge_geometry::border_point(hexagon, {150, -45.5});
    // where the line y = -(45.5 / 150) x meets the side from (102, 0) to (76, -45.5)
    const double t = (45.5 * 102 / 150) / (45.5 + 26 * 45.5 / 150);
    ASSERT_NEAR(102 - 26 * t, p.x, 1e-3);
    ASSERT_NEAR(-45.5 * t, p.y, 1e-3);
    // straight out of a corner and out of the flat top still end on the outline
    const Point right = edge_geometry::border_point(hexagon, {300, 0});
    ASSERT_NEAR(102, right.x, 1e-9);
    const Point up = edge_geometry::border_point(hexagon, {0, -300});
    ASSERT_NEAR(-45.5, up.y, 1e-9);
}

UTEST(edge_geometry, border_point_of_a_parallelogram_follows_the_lean) {
    // lean right, 186 x 36: the bottom edge runs from x = -93 to 75 relative to the box, the top from -75 to 93
    const NodeLayout lean = node(Shape::LeanRight, 0, 0, 186, 36);
    const Point right = edge_geometry::border_point(lean, {400, 0});
    ASSERT_NEAR(84, right.x, 1e-9);  // middle of the right slanted side
}

// utils.calcLabelPosition: the point half way along the polyline
UTEST(edge_geometry, label_position_is_half_way_along_the_path) {
    ASSERT_NEAR(5, edge_geometry::label_position({{0, 0}, {10, 0}}).x, 1e-9);
    // 10 + 30 long: half is 20, i.e. 10 into the second segment
    const Point p = edge_geometry::label_position({{0, 0}, {10, 0}, {10, 30}});
    ASSERT_NEAR(10, p.x, 1e-9);
    ASSERT_NEAR(10, p.y, 1e-9);
    // a single point is its own middle
    ASSERT_NEAR(7, edge_geometry::label_position({{7, 3}}).x, 1e-9);
    // interpolated coordinates are rounded to 5 decimals like roundNumber(x, 5)
    const Point q = edge_geometry::label_position({{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 2.0000001}});
    ASSERT_NEAR(1, q.x, 1e-9);
    ASSERT_NEAR(1.0, q.y, 1e-9);
}

// utils.isLabelCoordinateInPath: the rounded x or y of the point appears as text in the path, decimals rounded
UTEST(edge_geometry, label_coordinate_in_path_compares_rounded_numbers) {
    const std::string d = "M360,93.5L364.167,93.5C368.333,93.5";  // sanitised: M360,94L364,94C368,94
    ASSERT_TRUE(edge_geometry::is_label_coordinate_in_path({360.2, 10}, d));   // x rounds to 360
    ASSERT_TRUE(edge_geometry::is_label_coordinate_in_path({1, 94.4}, d));     // y rounds to 94
    ASSERT_FALSE(edge_geometry::is_label_coordinate_in_path({200, 200}, d));
    ASSERT_TRUE(edge_geometry::is_label_coordinate_in_path({3.6, 999}, d));    // "4" is a substring of "364"
}

// getBBox of the drawn path: the extent of the curve itself, not of its control polygon
UTEST(edge_geometry, curve_bounds_of_a_straight_path_are_its_points) {
    const Rect b = edge_geometry::curve_bounds("linear", {{0, 0}, {10, 5}, {20, 0}});
    ASSERT_NEAR(0, b.x, 1e-9);
    ASSERT_NEAR(0, b.y, 1e-9);
    ASSERT_NEAR(20, b.width, 1e-9);
    ASSERT_NEAR(5, b.height, 1e-9);
}

UTEST(edge_geometry, curve_bounds_of_a_basis_curve_stay_inside_the_corner_it_cuts) {
    // a peak: the basis spline reaches only (0 + 4 * 30 + 0) / 6 = 20 of the 30
    const Rect b = edge_geometry::curve_bounds("basis", {{0, 0}, {10, 30}, {20, 0}});
    ASSERT_NEAR(0, b.x, 1e-6);  // starts and ends on the end points
    ASSERT_NEAR(20, b.right(), 1e-6);
    ASSERT_NEAR(0, b.y, 1e-6);
    ASSERT_NEAR(20, b.bottom(), 1e-2);
}

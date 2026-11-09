// d3-shape's pie() and arc(), with the paths Mermaid 12.2.0 writes for the same slices.
#include <cmath>

#include "../../src/d3_shape.h"
#include "utest.h"

using namespace mermaid::detail;

UTEST(d3_shape, pie_shares_the_circle_by_the_values_in_their_order) {
    const auto slices = d3_pie({1, 3});
    ASSERT_EQ(2u, slices.size());
    EXPECT_EQ(0.0, slices[0].start_angle);
    EXPECT_NEAR(slices[0].end_angle, slices[1].start_angle, 1e-15);
    EXPECT_NEAR(2 * 3.141592653589793, slices[1].end_angle, 1e-12);
    EXPECT_NEAR(3.141592653589793 / 2, slices[0].end_angle, 1e-12);
    EXPECT_EQ(0.0, d3_pie({0, 5})[0].end_angle);  // a value of 0 has no angle
}

UTEST(d3_shape, an_arc_writes_a_sector_with_three_digits) {
    const auto slices = d3_pie({386, 85.5, 15});
    const d3_arc arc(0, 185);
    EXPECT_STREQ("M0,-185A185,185,0,1,1,-178.157,-49.85L0,0Z", arc.path(slices[0]).c_str());
    EXPECT_STREQ("M-178.157,-49.85A185,185,0,0,1,-35.616,-181.539L0,0Z", arc.path(slices[1]).c_str());
}

UTEST(d3_shape, a_whole_circle_is_two_half_arcs_and_a_donut_slice_has_an_inner_arc) {
    EXPECT_STREQ("M0,-185A185,185,0,1,1,0,185A185,185,0,1,1,0,-185Z", d3_arc(0, 185).path(d3_pie({7})[0]).c_str());
    const std::string donut = d3_arc(74, 185).path(d3_pie({1, 1})[0]);
    EXPECT_TRUE(donut.find("A74,74,0,1,0,") != std::string::npos);
}

UTEST(d3_shape, the_centroid_is_the_middle_of_the_slice_between_the_radii) {
    const auto [x, y] = d3_arc(100, 100).centroid({0, 3.141592653589793});  // the right half: its middle points right
    EXPECT_NEAR(100.0, x, 1e-9);
    EXPECT_NEAR(0.0, y, 1e-9);
}

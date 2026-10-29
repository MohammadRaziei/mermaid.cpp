#include <mermaid/css_color.h>

#include "utest.h"

using namespace mermaid;

UTEST(css_color, hex_rgb_and_hsl_are_parsed) {
    auto c = parse_css_color("#ddeeff");
    ASSERT_TRUE(c.has_value());
    ASSERT_EQ(221, c->r);
    ASSERT_EQ(238, c->g);
    ASSERT_EQ(255, c->b);
    ASSERT_EQ(255, parse_css_color("#fff")->g);
    ASSERT_EQ(51, parse_css_color("rgb(51, 0, 0)")->r);
    // the colour text goes through variables: utest prints the asserted expression as a printf format, and `%` breaks that
    const auto edge_label_background = parse_css_color("hsl(-120, 0%, 80%)");  // Mermaid's edge label background
    const auto red = parse_css_color("hsl(0, 100%, 50%)");
    const auto blue = parse_css_color("hsl(240, 100%, 50%)");
    ASSERT_EQ(204, edge_label_background->b);
    ASSERT_EQ(255, red->r);
    ASSERT_EQ(0, red->g);
    ASSERT_EQ(255, blue->b);
    ASSERT_EQ(255, parse_css_color("red")->r);  // keywords parse like khroma's
    ASSERT_FALSE(parse_css_color("#12").has_value());
}

UTEST(css_color, output_forms_match_the_browser_and_khroma) {
    ASSERT_STREQ("rgb(221, 238, 255)", css_rgb(*parse_css_color("#ddeeff")).c_str());
    ASSERT_STREQ("rgba(204, 204, 204, 0.5)", fade_color("hsl(-120, 0%, 80%)", 0.5).c_str());
    ASSERT_STREQ("rgba(255, 0, 0, 0.5)", fade_color("red", 0.5).c_str());
}

UTEST(css_color, keywords_parse_and_fade_like_khroma) {
    ASSERT_EQ(255, parse_css_color("white")->r);
    ASSERT_EQ(211, parse_css_color(" LightGrey ")->g);
    ASSERT_EQ(128, parse_css_color("green")->g);
    ASSERT_FALSE(parse_css_color("nocolour").has_value());
    ASSERT_STREQ("rgba(255, 255, 255, 0.5)", fade_color("white", 0.5).c_str());
}

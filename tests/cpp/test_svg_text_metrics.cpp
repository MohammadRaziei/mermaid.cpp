#include "../../src/svg_text_metrics.h"
#include "utest.h"

using mermaid::detail::svg_text_metrics;

// The expectations are what Chromium reported for the same strings with the pinned Liberation fonts (tools/dump_font_units.py
// and the reference sequence diagrams of tests/data/sequence).
UTEST(svg_text_metrics, serif_widths_are_whole_pixels_of_the_times_like_face) {
    EXPECT_EQ(213.0, svg_text_metrics::serif(std::string(30, 'a')).width);  // 0.444 em per `a`
    EXPECT_EQ(227.0, svg_text_metrics::serif(std::string(15, 'W')).width);
    EXPECT_EQ(240.0, svg_text_metrics::serif(std::string(30, 'x')).width);
    EXPECT_EQ(134.0, svg_text_metrics::serif(std::string(30, 'l')).width);
}

UTEST(svg_text_metrics, a_glyph_below_the_descent_makes_the_height_eighteen) {
    EXPECT_EQ(17.0, svg_text_metrics::serif("Hello Bob").height);
    EXPECT_EQ(18.0, svg_text_metrics::serif("solid point").height);
}

UTEST(svg_text_metrics, blanks_collapse_and_trim_like_an_svg_text) {
    EXPECT_EQ(svg_text_metrics::serif("a b").width, svg_text_metrics::serif("  a    b  ").width);
    EXPECT_EQ(0.0, svg_text_metrics::serif("   ").width);
}

UTEST(svg_text_metrics, the_serif_face_decides_unless_sans_is_bigger_in_every_dimension) {
    // `a`s: the sans face is wider but not taller, so the serif one answers
    EXPECT_EQ(213.0, svg_text_metrics::calculate(std::string(30, 'a')).width);
}

UTEST(svg_text_metrics, a_lone_Q_is_as_wide_in_both_faces_so_the_serif_height_stays) {
    // Chromium: serif `Q` is 12px wide, sans `Q` 12.45px with no overhang, so both round to 12 and sans is not bigger
    EXPECT_EQ(17.0, svg_text_metrics::calculate("Q").height);
    EXPECT_EQ(18.0, svg_text_metrics::calculate("Qa").height);  // here sans is wider as well
}

// Chromium's getComputedTextLength() and getBBox() for these strings at these sizes (the pinned Liberation faces); the width is
// rounded like calculateTextDimensions does, the height is the box's.
UTEST(svg_text_metrics, other_font_sizes_follow_chromium_to_the_pixel) {
    EXPECT_EQ(34.0, svg_text_metrics::serif("Z0t43O/6?9c:", 6).width);  // bbox 33.658
    EXPECT_EQ(6.0, svg_text_metrics::serif("Z0t43O/6?9c:", 6).height);
    EXPECT_EQ(229.0, svg_text_metrics::serif("ntfWALK2:", 48).width);
    EXPECT_EQ(53.0, svg_text_metrics::serif("ntfWALK2:", 48).height);
    EXPECT_EQ(27.0, svg_text_metrics::sans("ome", 14).width);  // bbox 27.448
    EXPECT_EQ(16.0, svg_text_metrics::sans("ome", 14).height);
    EXPECT_EQ(164.0, svg_text_metrics::sans("AJmxCwq(8LEc(", 22).width);  // bbox 164.481
    EXPECT_EQ(25.0, svg_text_metrics::sans("AJmxCwq(8LEc(", 22).height);
}

UTEST(svg_text_metrics, the_default_size_is_sixteen) {
    EXPECT_EQ(svg_text_metrics::calculate("Hello Bob", 16).width, svg_text_metrics::calculate("Hello Bob").width);
    EXPECT_EQ(17.0, svg_text_metrics::drawn_height("Hello"));
    EXPECT_EQ(18.0, svg_text_metrics::drawn_height("gyp"));
}

UTEST(svg_text_metrics, the_drawn_height_follows_the_size) {
    EXPECT_EQ(6.0, svg_text_metrics::drawn_height("Hello", 6));  // sans box at 6px
    EXPECT_TRUE(svg_text_metrics::drawn_height("Hello", 32) > svg_text_metrics::drawn_height("Hello", 16));
}

#include <mermaid/browser_text_measurer.h>

#include "../../src/svg_text_metrics.h"
#include "utest.h"

using mermaid::detail::svg_text_metrics;

// The expectations are what Chromium reported for the same strings with the pinned Liberation fonts (tools/dump_svg_text_metrics.mjs
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
    mermaid::browser_text_measurer sans(mermaid::TextStyle{16, 1.5});
    const svg_text_metrics metrics(sans);
    // `a`s: the sans face is wider but not taller, so the serif one answers
    EXPECT_EQ(213.0, metrics.calculate(std::string(30, 'a')).width);
}

UTEST(svg_text_metrics, a_lone_Q_is_as_wide_in_both_faces_so_the_serif_height_stays) {
    // Chromium: serif `Q` is 12px wide, sans `Q` 12.45px with no overhang, so both round to 12 and sans is not bigger
    mermaid::browser_text_measurer sans(mermaid::TextStyle{16, 1.5});
    const svg_text_metrics metrics(sans);
    EXPECT_EQ(17.0, metrics.calculate("Q").height);
    EXPECT_EQ(18.0, metrics.calculate("Qa").height);  // here sans is wider as well
}

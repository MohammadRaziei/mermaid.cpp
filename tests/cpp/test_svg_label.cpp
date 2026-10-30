#include <cmath>
#include <string_view>

#include "../../src/svg_label.h"
#include "utest.h"

using namespace mermaid;
using mermaid::detail::svg_label;

namespace {
// 1 px per byte, so the tests exercise only the layout of svg_label itself.
class FixedWidthMeasurer final : public TextMeasurer {
public:
    double line_width(std::string_view line) const override { return static_cast<double>(line.size()); }
};
}  // namespace

UTEST(svg_label, a_row_is_ascent_plus_descent_high_and_each_further_row_adds_one_point_one_em) {
    const FixedWidthMeasurer m;
    ASSERT_NEAR(16.0, svg_label::layout(m, "a", 100).size.height, 1e-9);
    ASSERT_NEAR(31.4, svg_label::layout(m, "a\nb", 100).size.height, 1e-9);
    ASSERT_NEAR(46.8, svg_label::layout(m, "a\nb\nc", 100).size.height, 1e-9);
    ASSERT_NEAR(1.0, svg_label::layout(m, "a", 100).top, 1e-9);
}

UTEST(svg_label, rows_wrap_greedily_at_spaces) {
    const FixedWidthMeasurer m;
    const svg_label label = svg_label::layout(m, "aaa bbb ccc", 10);
    ASSERT_EQ(2u, label.lines.size());
    ASSERT_EQ(2u, label.lines[0].size());
    ASSERT_STREQ("ccc", label.lines[1][0].text.c_str());
}

UTEST(svg_label, a_word_wider_than_the_label_breaks_between_characters) {
    const FixedWidthMeasurer m;
    const svg_label label = svg_label::layout(m, "abcdefghij", 4);
    ASSERT_EQ(3u, label.lines.size());
    ASSERT_STREQ("abcd", label.lines[0][0].text.c_str());
    ASSERT_STREQ("ij", label.lines[2][0].text.c_str());
}

UTEST(svg_label, the_box_reaches_as_far_as_the_last_glyph_does) {
    const FixedWidthMeasurer m;
    ASSERT_NEAR(1.0, svg_label::layout(m, "l", 100).size.width, 1e-9);  // l reaches nowhere
    ASSERT_NEAR(1.0 + 0.992, svg_label::layout(m, "k", 100).size.width, 1e-9);
    ASSERT_NEAR(-0.5, svg_label::layout(m, "l", 100).left, 1e-9);
}

UTEST(svg_label, words_keep_their_face) {
    const FixedWidthMeasurer m;
    const svg_label label = svg_label::layout_words(m, {{{"a", FontFace::Bold}, {"b", FontFace::Italic}}}, 100);
    ASSERT_EQ(1u, label.lines.size());
    ASSERT_TRUE(label.lines[0][0].face == FontFace::Bold);
    ASSERT_TRUE(label.lines[0][1].face == FontFace::Italic);
}

UTEST(svg_label, row_y_is_written_as_javascript_does) {
    ASSERT_STREQ("-0.1em", svg_label::row_y(0).c_str());
    ASSERT_STREQ("1em", svg_label::row_y(1).c_str());
    ASSERT_STREQ("2.1em", svg_label::row_y(2).c_str());
}

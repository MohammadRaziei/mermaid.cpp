#include <memory>
#include <string_view>

#include "text_measurer.h"
#include "utest.h"

using namespace mermaidcpp;

namespace {
// 1 px per byte, so the tests exercise only TextMeasurer's own logic.
class FixedWidthMeasurer final : public TextMeasurer {
public:
    explicit FixedWidthMeasurer(TextStyle s = {}) : TextMeasurer(s) {}
    double line_width(std::string_view line) const override {
        return static_cast<double>(line.size());
    }
};
}  // namespace

UTEST(text_style, defaults_match_mermaid_html_labels) {
    // upstream flowchart labels: 14px font, 21px (= 1.5 em) per line
    TextStyle s;
    ASSERT_EQ(14.0, s.font_size);
    ASSERT_EQ(1.5, s.line_height_em);
    ASSERT_EQ(21.0, s.line_height());
}

UTEST(text_measurer, single_line) {
    FixedWidthMeasurer m;
    Size s = m.measure("hello");
    ASSERT_EQ(5.0, s.width);
    ASSERT_EQ(21.0, s.height);
}

UTEST(text_measurer, multi_line_uses_widest_line_and_stacks_height) {
    FixedWidthMeasurer m;
    Size s = m.measure("ab\nabcd\nabc");
    ASSERT_EQ(4.0, s.width);
    ASSERT_EQ(63.0, s.height);
}

UTEST(text_measurer, empty_text_is_one_empty_line) {
    FixedWidthMeasurer m;
    Size s = m.measure("");
    ASSERT_EQ(0.0, s.width);
    ASSERT_EQ(21.0, s.height);
}

UTEST(text_measurer, trailing_newline_adds_a_line) {
    FixedWidthMeasurer m;
    ASSERT_EQ(42.0, m.measure("a\n").height);
}

UTEST(text_measurer, custom_style_changes_height) {
    FixedWidthMeasurer m(TextStyle{16.0, 1.25});
    ASSERT_EQ(20.0, m.measure("x").height);
}

UTEST(text_measurer, usable_through_the_interface) {
    std::unique_ptr<TextMeasurer> m = std::make_unique<FixedWidthMeasurer>();
    ASSERT_EQ(3.0, m->measure("abc").width);
}

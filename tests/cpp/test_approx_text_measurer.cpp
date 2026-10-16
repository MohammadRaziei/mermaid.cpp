#include <mermaid/approx_text_measurer.h>
#include "utest.h"

using namespace mermaid;

// Reference widths: Helvetica/Arial advance widths (1000 units/em) at 14 px.
// The "No" figure is also what real Mermaid 11.16 measured in a browser
// (foreignObject width 17.90625 in merman's upstream-svgs/flowchart/basic.svg).
UTEST(approx_text_measurer, ascii_widths_follow_arial_metrics) {
    ApproxTextMeasurer m;
    ASSERT_NEAR(17.892, m.line_width("No"), 1e-9);     // (722 + 556) * 14 / 1000
    ASSERT_NEAR(29.568, m.line_width("Start"), 1e-9);  // 2112 * 14 / 1000
    ASSERT_NEAR(3.892, m.line_width(" "), 1e-9);       // 278
}

UTEST(approx_text_measurer, close_to_real_browser_measurement) {
    ApproxTextMeasurer m;
    ASSERT_NEAR(17.90625, m.line_width("No"), 0.05);
}

UTEST(approx_text_measurer, scales_linearly_with_font_size) {
    ApproxTextMeasurer small(TextStyle{14.0, 1.5});
    ApproxTextMeasurer big(TextStyle{28.0, 1.5});
    ASSERT_NEAR(2.0 * small.line_width("Hello"), big.line_width("Hello"), 1e-9);
}

UTEST(approx_text_measurer, empty_line_has_zero_width) {
    ApproxTextMeasurer m;
    ASSERT_EQ(0.0, m.line_width(""));
}

UTEST(approx_text_measurer, non_ascii_uses_fallbacks) {
    ApproxTextMeasurer m;
    // 2-byte letters (e.g. "é") get an average Latin width, 3-byte (CJK) a full em
    ASSERT_NEAR(0.556 * 14.0, m.line_width("\xC3\xA9"), 1e-9);
    ASSERT_NEAR(14.0, m.line_width("\xE4\xB8\xAD"), 1e-9);
}

UTEST(approx_text_measurer, measure_combines_width_and_height) {
    ApproxTextMeasurer m;
    Size s = m.measure("No");
    ASSERT_NEAR(17.892, s.width, 1e-9);
    ASSERT_EQ(21.0, s.height);
}

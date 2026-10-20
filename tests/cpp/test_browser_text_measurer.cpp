#include <mermaid/browser_text_measurer.h>

#include "utest.h"

using namespace mermaid;

// Widths measured by headless Chromium (getBoundingClientRect of a span, 14px,
// font-family "Recursive Variable", arial, sans-serif): kerning is included, so
// "AV To Wa" is not the sum of its letters. tools/dump_font_metrics.mjs.
UTEST(browser_text_measurer, matches_the_browser_for_labels_of_the_fixtures) {
    browser_text_measurer m;
    ASSERT_NEAR(22.84375, m.line_width("Yes"), 1e-9);
    ASSERT_NEAR(17.90625, m.line_width("No"), 1e-9);
    ASSERT_NEAR(29.578125, m.line_width("Start"), 0.05);
    ASSERT_NEAR(43.59375, m.line_width("Choice"), 0.05);
}

UTEST(browser_text_measurer, applies_kerning) {
    browser_text_measurer m;
    ASSERT_NEAR(60.4375, m.line_width("AV To Wa"), 0.1);
    ASSERT_NEAR(51.859375, m.line_width("AVATAR"), 0.1);
    ASSERT_NEAR(121.40625, m.line_width("Tyrannosaurus Rex"), 0.1);
}

UTEST(browser_text_measurer, longer_texts_stay_within_a_tenth_of_a_pixel) {
    browser_text_measurer m;
    ASSERT_NEAR(277.03125, m.line_width("The quick brown fox jumps over the lazy dog"), 0.1);
    ASSERT_NEAR(184.203125, m.line_width("Some Longer Label To Check"), 0.1);
    ASSERT_NEAR(94.171875, m.line_width("Resolved: 42%"), 0.1);
}

UTEST(browser_text_measurer, scales_linearly_with_font_size) {
    browser_text_measurer narrow(TextStyle{14.0, 1.5});
    browser_text_measurer wide(TextStyle{28.0, 1.5});
    ASSERT_NEAR(2.0 * narrow.line_width("Hello"), wide.line_width("Hello"), 1e-9);
}

UTEST(browser_text_measurer, empty_line_has_zero_width) {
    browser_text_measurer m;
    ASSERT_EQ(0.0, m.line_width(""));
}

UTEST(browser_text_measurer, latin_1_letters_have_their_own_width_and_cjk_a_full_em) {
    browser_text_measurer m;
    ASSERT_TRUE(m.line_width("\xC3\xA9") > 6.0 && m.line_width("\xC3\xA9") < 9.0);  // e acute
    ASSERT_NEAR(14.0, m.line_width("\xE4\xB8\xAD"), 1e-9);                         // CJK: 1 em
}

UTEST(browser_text_measurer, measure_combines_width_and_height) {
    browser_text_measurer m;
    const Size s = m.measure("No");
    ASSERT_NEAR(17.90625, s.width, 1e-9);
    ASSERT_EQ(21.0, s.height);
}

// A label is HTML: runs of spaces collapse and spaces at the ends of the line vanish, as in the
// browser (the text itself keeps them; only its width does not count them).
UTEST(browser_text_measurer, collapses_runs_of_spaces_and_trims_the_ends) {
    browser_text_measurer m;
    ASSERT_NEAR(m.line_width("spaces preserved?"), m.line_width("spaces    preserved?"), 1e-9);
    ASSERT_NEAR(m.line_width("a"), m.line_width("  a  "), 1e-9);
    ASSERT_NEAR(m.line_width("a b"), m.line_width("a\t b"), 1e-9);
}

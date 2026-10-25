#include <mermaid/browser_text_measurer.h>

#include "../../src/markdown_label.h"
#include "utest.h"

using mermaid::FontFace;
using mermaid::detail::markdown_label;

UTEST(markdown_label, emphasis_becomes_strong_and_em) {
    ASSERT_STREQ("<p><strong>bold</strong> and <em>italic</em></p>",
                 markdown_label("**bold** and *italic*").html().c_str());
    ASSERT_STREQ("<p><strong>a</strong> <strong>b</strong> <em>c</em> <em>d</em></p>",
                 markdown_label("**a** __b__ *c* _d_").html().c_str());
    ASSERT_STREQ("<p><em><strong>both</strong></em></p>", markdown_label("***both***").html().c_str());
}

UTEST(markdown_label, lines_are_joined_with_br_and_text_is_escaped) {
    ASSERT_STREQ("<p>one<br />two<br />three</p>", markdown_label("one\n  two\n  three").html().c_str());
    ASSERT_STREQ("<p>a &lt; b &amp; c</p>", markdown_label("a < b & c").html().c_str());
}

UTEST(markdown_label, delimiters_that_do_not_open_or_close_stay_text) {
    ASSERT_STREQ("<p>2 * 3 * 4</p>", markdown_label("2 * 3 * 4").html().c_str());
    ASSERT_STREQ("<p>snake_case_name</p>", markdown_label("snake_case_name").html().c_str());
    ASSERT_STREQ("<p>**open</p>", markdown_label("**open").html().c_str());
}

UTEST(markdown_label, measuring_uses_the_face_of_each_run) {
    mermaid::browser_text_measurer m;
    // `**edge** x` as an edge label of tests/data/merman/s14_markdown_strings.mmd: 43.578125 in Chromium
    const auto edge = markdown_label("**edge** x").measure(m, 200);
    ASSERT_FALSE(edge.wrapped);
    ASSERT_NEAR(43.578125, edge.size.width, 0.1);
    ASSERT_NEAR(21, edge.size.height, 1e-9);
    // three lines; a long one wraps at 120 and the label is then 120 wide (as in the reference: 5 lines, 105px)
    ASSERT_NEAR(63, markdown_label("one\ntwo\nthree").measure(m, 120).size.height, 1e-9);
    const auto wide = markdown_label("hello *world* and **bold text** that is a rather long markdown label to wrap").measure(m, 120);
    ASSERT_TRUE(wide.wrapped);
    ASSERT_NEAR(120, wide.size.width, 1e-9);
    ASSERT_NEAR(105, wide.size.height, 1e-9);
}

UTEST(markdown_label, wrapped_table_counts_the_blank_after_the_longest_word) {
    mermaid::browser_text_measurer m;
    ASSERT_NEAR(121.421875, markdown_label("snake_case_name and 2 * 3 * 4").measure(m, 120).size.width, 0.1);
}

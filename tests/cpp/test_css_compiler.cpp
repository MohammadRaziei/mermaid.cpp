// css_compiler turns Mermaid's nested style templates into the flat, minified sheet the browser
// build produces (stylis). Expected strings below are taken from the real output.
#include <mermaid/css_compiler.h>
#include "utest.h"

using mermaid::css_compiler;

namespace {
std::string compile(const std::string& css) { return css_compiler::compile("#basic", css); }
}  // namespace

UTEST(css_compiler, the_root_selector_and_declarations_are_minified) {
    ASSERT_STREQ("#basic{font-family:\"Recursive Variable\",arial,sans-serif;font-size:14px;fill:#28253D;}",
                 compile("& { font-family: \"Recursive Variable\", arial, sans-serif; font-size: 14px; fill: #28253D }")
                     .c_str());
}

UTEST(css_compiler, rules_are_prefixed_with_the_namespace) {
    ASSERT_STREQ("#basic .a{x:y;}", compile(".a { x: y }").c_str());
    ASSERT_STREQ("#basic [data-look=\"neo\"].node rect{stroke:red;}",
                 compile("[data-look=\"neo\"].node rect { stroke: red }").c_str());
    ASSERT_STREQ("#basic :root{--mermaid-font-family:\"trebuchet ms\",verdana;}",
                 compile(":root { --mermaid-font-family: \"trebuchet ms\", verdana }").c_str());
}

UTEST(css_compiler, selector_lists_are_split_trimmed_and_prefixed_each) {
    ASSERT_STREQ("#basic .label text,#basic span{fill:#28253D;}", compile(".label text,span { fill: #28253D }").c_str());
    ASSERT_STREQ("#basic .node rect,#basic .node circle{a:b;}", compile(".node rect,\n  .node circle {a:b}").c_str());
    ASSERT_STREQ("#basic .a text,#basic .b{a:b;}", compile(".a text , .b { a: b }").c_str());
}

UTEST(css_compiler, nested_rules_follow_their_parents_declarations) {
    ASSERT_STREQ(
        "#basic .edgeLabel{background-color:hsl(-120, 0%, 80%);text-align:center;}"
        "#basic .edgeLabel p{background-color:hsl(-120, 0%, 80%);}"
        "#basic .edgeLabel rect{opacity:0.5;}",
        compile(".edgeLabel { background-color: hsl(-120, 0%, 80%); p { background-color: hsl(-120, 0%, 80%); } "
                "rect { opacity: 0.5; } text-align: center; }")
            .c_str());
}

UTEST(css_compiler, important_and_combinators_lose_their_spaces) {
    ASSERT_STREQ("#basic .root .anchor path{fill:#000!important;}", compile(".root .anchor path { fill: #000 !important; }").c_str());
    ASSERT_STREQ("#basic .cold>*{fill:red!important;}", compile(".cold > * { fill: red !important; }").c_str());
    ASSERT_STREQ("#basic .a{stroke-dasharray:9,5!important;}", compile(".a { stroke-dasharray: 9,5 !important }").c_str());
}

UTEST(css_compiler, comments_are_dropped) {
    ASSERT_STREQ("#basic .a{x:y;}", compile("/* c */ .a { x: y; // z\n }").c_str());
}

UTEST(css_compiler, values_keep_spaces_inside_parentheses_between_words) {
    ASSERT_STREQ("#basic .a{border:1px solid hsl(0, 0%, 90%);}", compile(".a { border: 1px solid hsl(0, 0%, 90%) }").c_str());
}

UTEST(css_compiler, keyframes_keep_their_inner_selectors_unprefixed) {
    ASSERT_STREQ("@keyframes dash{to{stroke-dashoffset:0;}}", compile("@keyframes dash { to { stroke-dashoffset: 0; } }").c_str());
    ASSERT_STREQ("@keyframes f{from{stroke-dashoffset:0;}}", compile("@keyframes f {\n from {\n stroke-dashoffset: 0;\n }\n }").c_str());
}

UTEST(css_compiler, an_ampersand_stands_for_the_parent_selector) {
    ASSERT_STREQ("#basic{a:b;}#basic .x{c:d;}", compile("& { a: b } & .x { c: d }").c_str());
}

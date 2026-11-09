// The pie parser against Mermaid 12.2.0's pie grammar (labels in quotes, unsigned decimal values, title and accessibility lines).
#include <mermaid/error.h>
#include <mermaid/pie_parser.h>

#include "utest.h"

using namespace mermaid::pie;

namespace {

Model parse(const std::string& text) { return Parser().parse(text); }

bool fails(const std::string& text) {
    try {
        parse(text);
    } catch (const mermaid::ParseError&) {
        return true;
    }
    return false;
}

}  // namespace

UTEST(pie_parser, sections_keep_their_order_and_a_repeated_label_keeps_its_first_value) {
    const Model m = parse("pie\n  \"Dogs\" : 386\n  \"Cats\" :85.5\n  \"Dogs\" : 1\n  \"a  b\":0\n");
    ASSERT_EQ(3u, m.sections.size());
    EXPECT_STREQ("Dogs", m.sections[0].label.c_str());
    EXPECT_EQ(386.0, m.sections[0].value);
    EXPECT_EQ(85.5, m.sections[1].value);
    EXPECT_STREQ("a  b", m.sections[2].label.c_str());
    EXPECT_FALSE(m.show_data);
}

UTEST(pie_parser, show_data_and_the_title_may_follow_pie_on_its_line_or_stand_alone) {
    const Model inline_title = parse("pie showData title Pets adopted\n\"A\" : 1\n");
    EXPECT_TRUE(inline_title.show_data);
    EXPECT_STREQ("Pets adopted", inline_title.title.c_str());
    EXPECT_STREQ("Later", parse("pie\n  title Later\n  \"A\" : 1\n").title.c_str());
    EXPECT_STREQ("Text wins", parse("---\ntitle: From frontmatter\n---\npie\n  title Text wins\n  \"A\" : 1\n").title.c_str());
    EXPECT_STREQ("From frontmatter", parse("---\ntitle: \"From frontmatter\"\n---\npie\n  \"A\" : 1\n").title.c_str());
}

UTEST(pie_parser, accessibility_lines_are_read) {
    const Model m = parse("pie\n  accTitle: A title\n  accDescr: A description\n  \"A\" : 1\n");
    EXPECT_STREQ("A title", m.acc_title.c_str());
    EXPECT_STREQ("A description", m.acc_descr.c_str());
    EXPECT_STREQ("Block\ntext", parse("pie\n  accDescr {\n  Block\n  text\n  }\n  \"A\" : 1\n").acc_descr.c_str());
}

UTEST(pie_parser, comments_and_init_directives_are_not_sections_and_the_init_pie_keys_are_read) {
    const Model m = parse("%%{init: {\"pie\": {\"legendPosition\": \"left\", \"donutHole\": 0.3, \"textPosition\": 0.5, \"highlightSlice\": \"A\"}}}%%\npie\n%% a comment\n\"A\" : 1\n");
    ASSERT_EQ(1u, m.sections.size());
    EXPECT_STREQ("left", m.config.legend_position->c_str());
    EXPECT_EQ(0.3, *m.config.donut_hole);
    EXPECT_EQ(0.5, *m.config.text_position);
    EXPECT_STREQ("A", m.config.highlight_slice->c_str());
    EXPECT_TRUE(m.config.unsupported.empty());
}

UTEST(pie_parser, a_theme_or_theme_variables_are_recorded_as_unsupported) {
    const Model m = parse("%%{init: {\"theme\": \"dark\", \"themeVariables\": {\"pie1\": \"#fff\"}}}%%\npie\n\"A\" : 1\n");
    ASSERT_EQ(2u, m.config.unsupported.size());
    EXPECT_TRUE(parse("%%{init: {\"theme\": \"default\"}}%%\npie\n\"A\" : 1\n").config.unsupported.empty());
}

UTEST(pie_parser, malformed_text_is_a_parse_error) {
    EXPECT_TRUE(fails("graph TD\nA-->B"));
    EXPECT_TRUE(fails("pie\n  Dogs : 3\n"));          // the label needs quotes
    EXPECT_TRUE(fails("pie\n  \"Dogs\" : -3\n"));       // no negative values
    EXPECT_TRUE(fails("pie\n  \"Dogs\" : 03\n"));       // no leading zero
    EXPECT_TRUE(fails("pie\n  \"Dogs\" : 3.\n"));
    EXPECT_TRUE(fails("pie\n  \"\" : 3\n"));
    EXPECT_TRUE(fails("pie extra\n"));
}

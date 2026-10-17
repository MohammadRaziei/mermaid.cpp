#include <mermaid/flowchart.h>
#include "utest.h"

using namespace mermaid::flowchart;

UTEST(flowchart_style, splits_declarations_in_order) {
    Style s = parse_style("fill:#f9f,stroke:#333,stroke-width:4px");
    ASSERT_EQ(3u, s.size());
    ASSERT_STREQ("fill", s[0].first.c_str());
    ASSERT_STREQ("#f9f", s[0].second.c_str());
    ASSERT_STREQ("stroke-width", s[2].first.c_str());
    ASSERT_STREQ("4px", s[2].second.c_str());
}

UTEST(flowchart_style, trims_and_drops_trailing_semicolon) {
    Style s = parse_style("  fill : #ddeeff ,  color:#001122;  ");
    ASSERT_EQ(2u, s.size());
    ASSERT_STREQ("fill", s[0].first.c_str());
    ASSERT_STREQ("#ddeeff", s[0].second.c_str());
    ASSERT_STREQ("#001122", s[1].second.c_str());
}

UTEST(flowchart_style, commas_inside_parentheses_do_not_split) {
    Style s = parse_style("fill:rgb(1,2,3),stroke:red");
    ASSERT_EQ(2u, s.size());
    ASSERT_STREQ("rgb(1,2,3)", s[0].second.c_str());
}

UTEST(flowchart_style, ignores_empty_and_malformed_pieces) {
    ASSERT_EQ(0u, parse_style("").size());
    ASSERT_EQ(1u, parse_style("fill:red,,nonsense").size());
}

UTEST(flowchart_names, shape_direction_and_edge_type_names) {
    ASSERT_STREQ("square", shape_name(Shape::Square));
    ASSERT_STREQ("", shape_name(Shape::Default));
    ASSERT_STREQ("TB", direction_name(Direction::TB));
    Edge e;
    ASSERT_TRUE(edge_type_name(e) == "arrow_point");
    e.end = Head::None;
    ASSERT_TRUE(edge_type_name(e) == "arrow_open");
    e.end = Head::Cross;
    ASSERT_TRUE(edge_type_name(e) == "arrow_cross");
    e.start = Head::Cross;
    ASSERT_TRUE(edge_type_name(e) == "double_arrow_cross");
    ASSERT_STREQ("dotted", stroke_name(Stroke::Dotted));
}

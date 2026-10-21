#include <algorithm>
#include <fstream>
#include <sstream>

#include <mermaid/error.h>
#include <mermaid/flowchart_parser.h>
#include "utest.h"

using namespace mermaid;
using namespace mermaid::flowchart;

namespace {

Model parse(const char* text) { return Parser().parse(text); }

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    if (s.empty()) return out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
    return out;
}

}  // namespace

UTEST(flowchart_parser, header_and_direction) {
    ASSERT_TRUE(Direction::TB == parse("graph TD\nA-->B").direction);
    ASSERT_TRUE(Direction::TB == parse("flowchart TB\nA-->B").direction);
    ASSERT_TRUE(Direction::BT == parse("flowchart BT\nA-->B").direction);
    ASSERT_TRUE(Direction::LR == parse("flowchart LR\nA-->B").direction);
    ASSERT_TRUE(Direction::RL == parse("graph RL;\nA-->B").direction);
    ASSERT_TRUE(Direction::TB == parse("graph\nA-->B").direction);
}

UTEST(flowchart_parser, rejects_other_diagrams_and_empty_input) {
    bool threw = false;
    try { parse("sequenceDiagram\nA->>B: hi"); } catch (const ParseError&) { threw = true; }
    ASSERT_TRUE(threw);
    threw = false;
    try { parse(""); } catch (const ParseError&) { threw = true; }
    ASSERT_TRUE(threw);
}

UTEST(flowchart_parser, node_shapes) {
    Model m = parse(
        "flowchart TD\n"
        "a[Sq]\nb(Ro)\nc([St])\nd[[Su]]\ne[(Cy)]\nf((Ci))\ng(((Dc)))\nh{Di}\ni{{Hx}}\n"
        "j>Od]\nk[/Lr/]\nl[\\Ll\\]\nm[/Tr\\]\nn[\\It/]\no\n");
    auto shape = [&](const char* id) { return m.find_node(id)->shape; };
    ASSERT_TRUE(Shape::Square == shape("a"));
    ASSERT_TRUE(Shape::Round == shape("b"));
    ASSERT_TRUE(Shape::Stadium == shape("c"));
    ASSERT_TRUE(Shape::Subroutine == shape("d"));
    ASSERT_TRUE(Shape::Cylinder == shape("e"));
    ASSERT_TRUE(Shape::Circle == shape("f"));
    ASSERT_TRUE(Shape::DoubleCircle == shape("g"));
    ASSERT_TRUE(Shape::Diamond == shape("h"));
    ASSERT_TRUE(Shape::Hexagon == shape("i"));
    ASSERT_TRUE(Shape::Odd == shape("j"));
    ASSERT_TRUE(Shape::LeanRight == shape("k"));
    ASSERT_TRUE(Shape::LeanLeft == shape("l"));
    ASSERT_TRUE(Shape::Trapezoid == shape("m"));
    ASSERT_TRUE(Shape::InvTrapezoid == shape("n"));
    ASSERT_TRUE(Shape::Default == shape("o"));
    ASSERT_STREQ("Di", m.find_node("h")->label.c_str());
    ASSERT_STREQ("o", m.find_node("o")->label.c_str());  // label defaults to id
}

UTEST(flowchart_parser, quoted_labels_keep_brackets_and_markdown_ticks_go) {
    Model m = parse("flowchart LR\na[\"brackets: [x] {y} (z)\"] --> b[\"`**bold**`\"]");
    ASSERT_STREQ("brackets: [x] {y} (z)", m.find_node("a")->label.c_str());
    ASSERT_STREQ("**bold**", m.find_node("b")->label.c_str());
}

UTEST(flowchart_parser, br_becomes_newline_and_entities_decode) {
    Model m = parse("flowchart TD\na[\"one<br/>two\"]\nb[\"x #quot;y#quot; #35;\"]");
    ASSERT_STREQ("one\ntwo", m.find_node("a")->label.c_str());
    ASSERT_STREQ("x \"y\" #", m.find_node("b")->label.c_str());
}

UTEST(flowchart_parser, edge_kinds) {
    Model m = parse(
        "flowchart TD\n"
        "A --> B\nA --- C\nA -.-> D\nA ==> E\nA --x F\nA --o G\nA <--> H\nA ~~~ I\n"
        "A -.- J\nA === K\nA x--x L\nA o--o M\n");
    ASSERT_EQ(12u, m.edges.size());
    auto type = [&](size_t i) { return edge_type_name(m.edges[i]); };
    ASSERT_TRUE(type(0) == "arrow_point");
    ASSERT_TRUE(type(1) == "arrow_open");
    ASSERT_TRUE(Stroke::Dotted == m.edges[2].stroke);
    ASSERT_TRUE(type(2) == "arrow_point");
    ASSERT_TRUE(Stroke::Thick == m.edges[3].stroke);
    ASSERT_TRUE(type(4) == "arrow_cross");
    ASSERT_TRUE(type(5) == "arrow_circle");
    ASSERT_TRUE(type(6) == "double_arrow_point");
    ASSERT_TRUE(Stroke::Invisible == m.edges[7].stroke);
    ASSERT_TRUE(Stroke::Dotted == m.edges[8].stroke);
    ASSERT_TRUE(type(8) == "arrow_open");
    ASSERT_TRUE(Stroke::Thick == m.edges[9].stroke);
    ASSERT_TRUE(type(10) == "double_arrow_cross");
    ASSERT_TRUE(type(11) == "double_arrow_circle");
}

UTEST(flowchart_parser, edge_length_counts_extra_dashes) {
    Model m = parse("flowchart TD\nA --> B\nA ---> C\nA ----> D");
    ASSERT_EQ(1, m.edges[0].length);
    ASSERT_EQ(2, m.edges[1].length);
    ASSERT_EQ(3, m.edges[2].length);
}

UTEST(flowchart_parser, edge_labels_in_all_syntaxes) {
    Model m = parse(
        "flowchart TD\n"
        "A -->|pipe| B\nA -- plain text --> C\nA == thick text ==> D\nA -. dotted text .-> E\n"
        "A -->|\"quoted | pipe\"| F\n");
    ASSERT_STREQ("pipe", m.edges[0].label.c_str());
    ASSERT_STREQ("plain text", m.edges[1].label.c_str());
    ASSERT_STREQ("thick text", m.edges[2].label.c_str());
    ASSERT_TRUE(Stroke::Thick == m.edges[2].stroke);
    ASSERT_STREQ("dotted text", m.edges[3].label.c_str());
    ASSERT_TRUE(Stroke::Dotted == m.edges[3].stroke);
    ASSERT_STREQ("quoted | pipe", m.edges[4].label.c_str());
}

UTEST(flowchart_parser, chains_and_ampersand_groups) {
    Model m = parse("flowchart TD\nA --> B --> C\nX & Y --> Z & W");
    ASSERT_EQ(2u + 4u, m.edges.size());
    ASSERT_STREQ("A", m.edges[0].from.c_str());
    ASSERT_STREQ("C", m.edges[1].to.c_str());
    ASSERT_STREQ("X", m.edges[2].from.c_str());
    ASSERT_STREQ("Z", m.edges[2].to.c_str());
    ASSERT_STREQ("W", m.edges[3].to.c_str());
    ASSERT_STREQ("Y", m.edges[4].from.c_str());
}

UTEST(flowchart_parser, edge_ids_auto_and_user_defined) {
    Model m = parse("flowchart TD\nA --> B\nA --> B\nC e1@--> D\ne1@{curve: basis}");
    ASSERT_STREQ("L_A_B_0", m.edges[0].id.c_str());
    ASSERT_STREQ("L_A_B_1", m.edges[1].id.c_str());
    ASSERT_STREQ("e1", m.edges[2].id.c_str());
}

UTEST(flowchart_parser, semicolons_comments_and_directives) {
    Model m = parse(
        "%%{init: {\"flowchart\": {\"nodeSpacing\": 20}}}%%\n"
        "flowchart TD\n"
        "%% a comment\n"
        "A --> B; B --> C %% trailing comment\n"
        "\n"
        "C --> D\n");
    ASSERT_EQ(3u, m.edges.size());
    ASSERT_EQ(4u, m.nodes.size());
}

UTEST(flowchart_parser, frontmatter_is_skipped) {
    Model m = parse("---\ntitle: demo\n---\nflowchart TD\nA --> B");
    ASSERT_EQ(1u, m.edges.size());
}

UTEST(flowchart_parser, redefinition_overrides_label_and_shape) {
    Model m = parse("flowchart TD\nA --> B\nA[Start]\nB{Pick}");
    ASSERT_EQ(2u, m.nodes.size());
    ASSERT_STREQ("Start", m.find_node("A")->label.c_str());
    ASSERT_TRUE(Shape::Diamond == m.find_node("B")->shape);
}

UTEST(flowchart_parser, node_data_block_sets_label_and_shape) {
    Model m = parse("flowchart LR\nA@{ label: \"A: (Edge Text)\" }\nB@{ shape: diamond, label: \"Pick\" }");
    ASSERT_STREQ("A: (Edge Text)", m.find_node("A")->label.c_str());
    ASSERT_TRUE(Shape::Default == m.find_node("A")->shape);
    ASSERT_TRUE(Shape::Diamond == m.find_node("B")->shape);
    ASSERT_STREQ("Pick", m.find_node("B")->label.c_str());
}

UTEST(flowchart_parser, classes_styles_and_link_styles) {
    Model m = parse(
        "flowchart TD\n"
        "classDef hot fill:#fdd,stroke:#c00;\n"
        "classDef cold fill:#def\n"
        "A:::hot --> B\n"
        "class B,C cold\n"
        "style A stroke-width:3px\n"
        "linkStyle 0 stroke:#0066cc,stroke-width:2px\n");
    ASSERT_EQ(2u, m.class_defs.size());
    ASSERT_EQ(2u, m.class_defs.at("hot").style.size());
    ASSERT_EQ(1u, m.find_node("A")->classes.size());
    ASSERT_STREQ("hot", m.find_node("A")->classes[0].c_str());
    ASSERT_STREQ("cold", m.find_node("B")->classes[0].c_str());
    ASSERT_STREQ("cold", m.find_node("C")->classes[0].c_str());
    ASSERT_STREQ("stroke-width", m.find_node("A")->style[0].first.c_str());
    ASSERT_EQ(3u, m.edges[0].style.size());  // flowDb adds fill:none to an indexed linkStyle
    ASSERT_STREQ("fill", m.edges[0].style[2].first.c_str());
    ASSERT_STREQ("none", m.edges[0].style[2].second.c_str());
}

UTEST(flowchart_parser, indexed_link_style_replaces_and_default_comes_first) {
    Model m = parse("flowchart TD\nA --> B\nB --> C\nlinkStyle 0 stroke:red\nlinkStyle default stroke:blue\nlinkStyle 0 stroke:green,fill:#fff");
    ASSERT_EQ(3u, m.edges[0].style.size());  // default + the last linkStyle 0 (it sets a fill, so no fill:none)
    ASSERT_STREQ("blue", m.edges[0].style[0].second.c_str());
    ASSERT_STREQ("green", m.edges[0].style[1].second.c_str());
    ASSERT_EQ(1u, m.edges[1].style.size());
}

UTEST(flowchart_parser, link_style_default_applies_to_every_edge) {
    Model m = parse("flowchart TD\nA --> B\nB --> C\nlinkStyle default stroke:red");
    ASSERT_EQ(1u, m.edges[0].style.size());
    ASSERT_EQ(1u, m.edges[1].style.size());
}

UTEST(flowchart_parser, subgraphs_nest_and_collect_members) {
    Model m = parse(
        "flowchart TD\n"
        "subgraph one [First]\n"
        "  direction LR\n"
        "  a --> b\n"
        "  subgraph two\n"
        "    c\n"
        "  end\n"
        "end\n"
        "d --> a\n");
    ASSERT_EQ(2u, m.subgraphs.size());
    const Subgraph* one = m.find_subgraph("one");
    const Subgraph* two = m.find_subgraph("two");
    ASSERT_TRUE(one != nullptr && two != nullptr);
    ASSERT_STREQ("First", one->title.c_str());
    ASSERT_STREQ("two", two->title.c_str());
    ASSERT_TRUE(one->direction.has_value());
    ASSERT_TRUE(Direction::LR == *one->direction);
    ASSERT_EQ(3u, one->members.size());  // a, b, two
    ASSERT_EQ(1u, two->members.size());
    ASSERT_STREQ("c", two->members[0].c_str());
}

UTEST(flowchart_parser, quoted_subgraph_titles) {
    Model m = parse("flowchart TD\nsubgraph S[\"Settings\"]\n a\nend\nsubgraph \"Free text\"\n b\nend");
    ASSERT_STREQ("Settings", m.find_subgraph("S")->title.c_str());
    ASSERT_STREQ("Free text", m.subgraphs[1].title.c_str());
}

UTEST(flowchart_parser, accessibility_fields) {
    Model m = parse("flowchart TD\naccTitle: My title\naccDescr: My description\nA-->B");
    ASSERT_STREQ("My title", m.acc_title.c_str());
    ASSERT_STREQ("My description", m.acc_descr.c_str());
}

UTEST(flowchart_parser, errors_carry_line_numbers) {
    int line = 0;
    try { parse("flowchart TD\nA --> B\nA --> \n"); } catch (const ParseError& e) { line = e.line(); }
    ASSERT_EQ(3, line);
    line = 0;
    try { parse("flowchart TD\nsubgraph x\nA"); } catch (const ParseError& e) { line = e.line(); }
    ASSERT_TRUE(line > 0);  // unterminated subgraph
}

// Semantic parity with merman: every fixture in tests/data/merman.
UTEST(flowchart_parser, matches_merman_semantic_goldens) {
    const std::string dir = MERMAID_TEST_DATA_DIR;
    std::string tsv = read_file(dir + "/flowchart_expected.tsv");
    ASSERT_TRUE(!tsv.empty());
    int checked = 0;
    for (const std::string& line : split(tsv, '\n')) {
        if (line.empty()) continue;
        std::vector<std::string> cols = split(line, '\t');
        // `split` drops a trailing empty field; pad to 5 columns.
        while (cols.size() < 5) cols.push_back("");
        std::string src = read_file(dir + "/merman/" + cols[0] + ".mmd");
        ASSERT_TRUE(!src.empty());
        Model m = Parser().parse(src);

        ASSERT_STREQ(cols[1].c_str(), direction_name(m.direction));

        std::vector<std::string> got_nodes;
        for (const Node& n : m.nodes)
            got_nodes.push_back(n.id + "\x1f" + n.label + "\x1f" + shape_name(n.shape));
        std::sort(got_nodes.begin(), got_nodes.end());
        std::vector<std::string> want_nodes = split(cols[2], '|');
        ASSERT_EQ(want_nodes.size(), got_nodes.size());
        for (size_t i = 0; i < want_nodes.size(); ++i)
            ASSERT_TRUE(want_nodes[i] == got_nodes[i]);

        std::vector<std::string> want_edges = split(cols[3], '|');
        ASSERT_EQ(want_edges.size(), m.edges.size());
        for (size_t i = 0; i < want_edges.size(); ++i) {
            const Edge& e = m.edges[i];
            std::string got = e.from + "\x1f" + e.to + "\x1f" + e.label + "\x1f" +
                              edge_type_name(e) + "\x1f" + stroke_name(e.stroke);
            ASSERT_TRUE(want_edges[i] == got);
        }

        std::vector<std::string> got_subs;
        for (const Subgraph& s : m.subgraphs) {
            std::vector<std::string> mem = s.members;
            std::sort(mem.begin(), mem.end());
            std::string joined;
            for (size_t i = 0; i < mem.size(); ++i) joined += (i ? "," : "") + mem[i];
            got_subs.push_back(s.id + "\x1f" + s.title + "\x1f" + joined);
        }
        std::sort(got_subs.begin(), got_subs.end());
        std::vector<std::string> want_subs = split(cols[4], '|');
        ASSERT_EQ(want_subs.size(), got_subs.size());
        for (size_t i = 0; i < want_subs.size(); ++i)
            ASSERT_TRUE(want_subs[i] == got_subs[i]);
        ++checked;
    }
    ASSERT_TRUE(checked >= 10);
}

// Mermaid's flowDb gives every vertex the DOM id `flowchart-<id>-<n>` where n is the number of
// addVertex calls made before the vertex was first seen (every mention counts, repeated or not).
UTEST(flowchart_parser, dom_index_counts_every_vertex_mention_before_the_first_one) {
    Model m = Parser().parse("flowchart TD\nA[Start] --> B{Choice}\nB -->|Yes| C[OK]\nB -->|No| D[Fail]");
    ASSERT_EQ(0, m.find_node("A")->dom_index);
    ASSERT_EQ(1, m.find_node("B")->dom_index);
    ASSERT_EQ(3, m.find_node("C")->dom_index);
    ASSERT_EQ(5, m.find_node("D")->dom_index);
}

UTEST(flowchart_parser, dom_index_counts_ampersand_groups_and_style_statements) {
    Model m = Parser().parse("flowchart TD\nA & B --> C\nstyle C fill:#fff\nC --> D");
    ASSERT_EQ(0, m.find_node("A")->dom_index);
    ASSERT_EQ(1, m.find_node("B")->dom_index);
    ASSERT_EQ(2, m.find_node("C")->dom_index);
    ASSERT_EQ(5, m.find_node("D")->dom_index);  // A B C, style C, then C D
}

// Mermaid keeps classes in the order they were first defined; the stylesheet follows that order.
UTEST(flowchart_parser, class_defs_keep_their_first_definition_order) {
    Model m = Parser().parse("flowchart TD\nclassDef hot fill:#f00\nclassDef cold fill:#00f\nclassDef hot,mild fill:#0f0\nA-->B");
    ASSERT_EQ(3u, m.class_def_order.size());
    ASSERT_STREQ("hot", m.class_def_order[0].c_str());
    ASSERT_STREQ("cold", m.class_def_order[1].c_str());
    ASSERT_STREQ("mild", m.class_def_order[2].c_str());
}

UTEST(flowchart_parser, edge_data_block_sets_the_curve_of_that_edge) {
    Model m = parse("flowchart TD\nA e1@--> B\nB e2@--> C\nB --> D\ne1@{curve: cardinal}\ne2@{ curve: linear }");
    ASSERT_STREQ("cardinal", m.edges[0].curve.c_str());
    ASSERT_STREQ("linear", m.edges[1].curve.c_str());
    ASSERT_STREQ("", m.edges[2].curve.c_str());
    ASSERT_EQ(4u, m.nodes.size());  // the data blocks did not create nodes
}

UTEST(flowchart_parser, a_class_given_twice_stays_twice_like_in_flowdb) {
    Model m = parse("flowchart TD\nA:::cold --> B\nclass A cold");
    ASSERT_EQ(2u, m.nodes[0].classes.size());
}

// ---- %%{init}%% directives (the parts the flowchart renderer reads)

UTEST(flowchart_init, theme_variables_and_flowchart_spacing) {
    const Model m = Parser().parse(
        "%%{init: {\"themeVariables\": {\"fontFamily\": \"arial\", \"clusterBkg\": \"#f8f8f8\", \"clusterBorder\": \"#999999\"},"
        " \"flowchart\": {\"nodeSpacing\": 70, \"rankSpacing\": 60}} }%%\nflowchart TD\nA-->B\n");
    ASSERT_TRUE(m.config.font_family.has_value());
    ASSERT_STREQ("arial", m.config.font_family->c_str());
    ASSERT_STREQ("#f8f8f8", m.config.cluster_bkg->c_str());
    ASSERT_STREQ("#999999", m.config.cluster_border->c_str());
    ASSERT_EQ(70.0, *m.config.node_spacing);
    ASSERT_EQ(60.0, *m.config.rank_spacing);
}

UTEST(flowchart_init, multi_line_directive_and_later_directive_wins) {
    const Model m = Parser().parse(
        "%%{\n  init: {\n    \"flowchart\": {\n      \"nodeSpacing\": 20,\n      \"rankSpacing\": 5\n    }\n  }\n}%%\n"
        "%%{initialize: {\"flowchart\": {\"nodeSpacing\": 33}}}%%\nflowchart LR\nA-->B\n");
    ASSERT_EQ(33.0, *m.config.node_spacing);
    ASSERT_EQ(5.0, *m.config.rank_spacing);
}

UTEST(flowchart_init, absent_unknown_or_broken_directives_leave_defaults) {
    const Model none = Parser().parse("flowchart TD\nA-->B\n");
    ASSERT_FALSE(none.config.font_family.has_value());
    ASSERT_FALSE(none.config.node_spacing.has_value());
    const Model odd = Parser().parse("%%{init: {\"theme\": \"forest\", \"flowchart\": {\"curve\": \"basis\"}}}%%\nflowchart TD\nA-->B\n");
    ASSERT_FALSE(odd.config.node_spacing.has_value());
    const Model broken = Parser().parse("%%{init: {\"flowchart\": {\"nodeSpacing\": }}}%%\nflowchart TD\nA-->B\n");
    ASSERT_FALSE(broken.config.node_spacing.has_value());
}

// ---- `A@{ label: ... }` nodes

UTEST(flowchart_node_data, data_block_advances_the_vertex_counter_twice) {
    const Model m = Parser().parse("flowchart LR\nA@{ label: \"x\" }\nB@{ label: \"y\" }\nC@{ label: \"z\" }\nA --> B\nB --> C\n");
    ASSERT_EQ(0, m.find_node("A")->dom_index);
    ASSERT_EQ(2, m.find_node("B")->dom_index);
    ASSERT_EQ(4, m.find_node("C")->dom_index);
}

UTEST(flowchart_node_data, label_from_a_data_block_is_markdown_unless_told_otherwise) {
    const Model m = Parser().parse(
        "flowchart LR\nA@{ label: \"a\" }\nB@{ label: \"b\", labelType: \"text\" }\nC[\"c\"]\nD@{ shape: rect }\n");
    ASSERT_TRUE(m.find_node("A")->markdown_label);
    ASSERT_FALSE(m.find_node("B")->markdown_label);
    ASSERT_FALSE(m.find_node("C")->markdown_label);
    ASSERT_FALSE(m.find_node("D")->markdown_label);  // no label given
}

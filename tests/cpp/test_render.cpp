#include <fstream>
#include <sstream>
#include <stack>
#include <stdexcept>

#include <mermaid/error.h>
#include <mermaid/mermaid.h>
#include <mermaid/render.h>
#include "utest.h"

using namespace mermaid;

namespace {

bool has(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

size_t count(const std::string& hay, const std::string& needle) {
    size_t n = 0, pos = 0;
    while ((pos = hay.find(needle, pos)) != std::string::npos) { ++n; pos += needle.size(); }
    return n;
}

// Minimal well-formedness check: tags balance and attributes are quoted.
bool well_formed(const std::string& s) {
    std::stack<std::string> open;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '<') { ++i; continue; }
        size_t end = s.find('>', i);
        if (end == std::string::npos) return false;
        std::string tag = s.substr(i + 1, end - i - 1);
        i = end + 1;
        if (tag.empty()) return false;
        if (tag[0] == '?' || tag[0] == '!') continue;
        if (tag[0] == '/') {
            if (open.empty() || open.top() != tag.substr(1)) return false;
            open.pop();
            continue;
        }
        size_t sp = tag.find_first_of(" \n/");
        std::string name = tag.substr(0, sp);
        if (tag.back() != '/') open.push(name);
        if (count(tag, "\"") % 2 != 0) return false;
    }
    return open.empty();
}

std::string read_fixture(const std::string& name) {
    std::ifstream in(std::string(MERMAID_TEST_DATA_DIR) + "/merman/" + name + ".mmd", std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

UTEST(render, produces_a_well_formed_svg_with_mermaid_viewbox) {
    std::string svg = render_svg(read_fixture("basic"));
    ASSERT_TRUE(svg.rfind("<svg", 0) == 0);
    ASSERT_TRUE(well_formed(svg));
    ASSERT_TRUE(has(svg, "viewBox=\"0 0 370 398\""));
    ASSERT_TRUE(has(svg, "xmlns=\"http://www.w3.org/2000/svg\""));
    ASSERT_TRUE(has(svg, ">Start<"));
    ASSERT_TRUE(has(svg, ">Choice<"));
    ASSERT_TRUE(has(svg, ">Yes<"));
    ASSERT_TRUE(has(svg, "<polygon"));  // diamond
}

UTEST(render, all_fixtures_render_well_formed) {
    const char* names[] = {
        "basic", "class_style", "stress_flowchart_amp_groups_edge_ids_034",
        "stress_flowchart_classdef_and_inline_classes_003",
        "stress_flowchart_comments_and_directives_010",
        "stress_flowchart_deeply_nested_clusters_019",
        "stress_flowchart_dense_parallel_edges_002",
        "stress_flowchart_directives_comments_blank_lines_032",
        "stress_flowchart_edge_label_position_064",
        "stress_flowchart_escape_sequences_and_quotes_012"};
    for (const char* name : names) {
        std::string svg = render_svg(read_fixture(name));
        ASSERT_TRUE(well_formed(svg));
        // (the stylesheet legitimately contains words like "infinite")
        ASSERT_TRUE(!has(svg, "=\"nan") && !has(svg, "=\"-nan") && !has(svg, "(nan") && !has(svg, ",nan") &&
                    !has(svg, " nan") && !has(svg, "=\"inf") && !has(svg, "=\"-inf") && !has(svg, ",inf") && !has(svg, " inf "));
    }
}

UTEST(render, escapes_xml_in_labels) {
    std::string svg = render_svg("flowchart TD\nA[\"a<b & \\\"c\\\" > d\"]");
    ASSERT_TRUE(well_formed(svg));
    ASSERT_TRUE(has(svg, "a&lt;b &amp;"));
    ASSERT_TRUE(!has(svg, "a<b"));
}

UTEST(render, themes_change_colours_and_unknown_theme_throws) {
    RenderOptions dark;
    dark.theme = "dark";
    std::string d = render_svg("flowchart TD\nA-->B", dark);
    std::string def = render_svg("flowchart TD\nA-->B");
    ASSERT_TRUE(d != def);
    ASSERT_TRUE(has(def, "#28253D"));  // the default theme is redux-color (neo)
    for (const char* t : {"forest", "neutral"}) {
        RenderOptions o;
        o.theme = t;
        ASSERT_TRUE(well_formed(render_svg("flowchart TD\nA-->B", o)));
    }
    bool threw = false;
    RenderOptions bad;
    bad.theme = "nope";
    try { render_svg("flowchart TD\nA-->B", bad); } catch (const std::invalid_argument&) { threw = true; }
    ASSERT_TRUE(threw);
}

UTEST(render, background_id_and_css_options) {
    RenderOptions o;
    o.background = "transparent";
    o.svg_id = "diagram-1";
    o.css = ".node{opacity:.5}";
    std::string svg = render_svg("flowchart TD\nA-->B", o);
    ASSERT_TRUE(has(svg, "id=\"diagram-1\""));
    ASSERT_TRUE(has(svg, "background-color: transparent"));
    ASSERT_TRUE(has(svg, "#diagram-1 .node{opacity:.5;}"));  // user css is compiled under the diagram id
}

UTEST(render, class_and_inline_styles_reach_the_shapes) {
    std::string svg = render_svg(
        "flowchart TD\nclassDef hot fill:#ffdddd,stroke:#cc0000,color:#330000\n"
        "A:::hot --> B\nstyle B fill:#abcdef\nlinkStyle 0 stroke:#0066cc");
    ASSERT_TRUE(has(svg, "fill:#ffdddd !important"));
    ASSERT_TRUE(has(svg, "stroke:#cc0000 !important"));
    ASSERT_TRUE(has(svg, "color:#330000 !important"));  // text colour (label group and span)
    ASSERT_TRUE(has(svg, "color: rgb(51, 0, 0) !important"));  // as the browser reports it on the div
    ASSERT_TRUE(has(svg, "fill:#abcdef !important"));
    ASSERT_TRUE(has(svg, "stroke:#0066cc"));
}

UTEST(render, arrow_heads_and_invisible_edges) {
    std::string svg = render_svg("flowchart TD\nA <--> B\nB --x C\nC ~~~ D");
    ASSERT_TRUE(has(svg, "marker-start"));
    ASSERT_TRUE(has(svg, "marker-end"));
    ASSERT_TRUE(has(svg, "crossEnd"));
    ASSERT_EQ(2u, count(svg, "flowchart-link\""));  // the ~~~ edge is not drawn
}

UTEST(render, every_shape_renders) {
    std::string svg = render_svg(
        "flowchart TD\na[Sq]\nb(Ro)\nc([St])\nd[[Su]]\ne[(Cy)]\nf((Ci))\ng(((Dc)))\nh{Di}\n"
        "i{{Hx}}\nj>Od]\nk[/Lr/]\nl[\\Ll\\]\nm[/Tr\\]\nn[\\It/]\no");
    ASSERT_TRUE(well_formed(svg));
    for (const char* label : {"Sq", "Ro", "St", "Su", "Cy", "Ci", "Dc", "Di", "Hx", "Od", "Lr", "Ll", "Tr", "It"})
        ASSERT_TRUE(has(svg, std::string(">") + label + "<"));
}

UTEST(render, accessibility_title_and_description) {
    std::string svg = render_svg("flowchart TD\naccTitle: My title\naccDescr: My desc\nA-->B");
    ASSERT_TRUE(has(svg, "<title"));
    ASSERT_TRUE(has(svg, "My title"));
    ASSERT_TRUE(has(svg, "<desc"));
    ASSERT_TRUE(has(svg, "My desc"));
}

UTEST(render, multiline_labels_are_one_html_paragraph_with_line_breaks) {
    std::string svg = render_svg("flowchart TD\nA[\"one<br/>two<br/>three\"]");
    ASSERT_TRUE(has(svg, "<p>one<br />two<br />three</p>"));
}

UTEST(render, subgraph_titles_are_drawn) {
    std::string svg = render_svg("flowchart TD\nsubgraph S[\"Settings\"]\n a --> b\nend");
    ASSERT_TRUE(has(svg, ">Settings<"));
    ASSERT_TRUE(has(svg, "class=\"cluster"));
}

UTEST(render, unsupported_and_invalid_input) {
    bool unsupported = false;
    try { render_svg("sequenceDiagram\nA->>B: hi"); } catch (const UnsupportedDiagram&) { unsupported = true; }
    ASSERT_TRUE(unsupported);
    bool parse_error = false;
    try { render_svg("not a diagram at all"); } catch (const ParseError&) { parse_error = true; }
    ASSERT_TRUE(parse_error);
}

UTEST(render, init_directive_and_frontmatter_do_not_break_detection) {
    std::string svg = render_svg("---\ntitle: x\n---\n%%{init: {\"theme\":\"dark\"}}%%\ngraph TD\nA-->B");
    ASSERT_TRUE(well_formed(svg));
}

UTEST(render, edge_style_text_follows_mermaid) {
    const std::string svg = render_svg("flowchart TD\nA --> B\nA -.-> C\nlinkStyle 0 stroke:#333,stroke-width:2px");
    ASSERT_TRUE(has(svg, ";stroke:#333;stroke-width:2px;fill:none;;;stroke:#333;stroke-width:2px;fill:none\""));
    ASSERT_TRUE(has(svg, "stroke-dashoffset: 0;;\""));          // no linkStyle: pathStyle is ";"
    ASSERT_TRUE(has(svg, "edge-pattern-dotted edge-thickness-normal edge-pattern-solid flowchart-link"));
    ASSERT_TRUE(has(svg, "pointEnd-margin__333"));
}

// shapes/cylinder.ts: the path is drawn from 0,0 and moved into place; the label sits lower by padding / 1.5.
UTEST(render, cylinder_follows_mermaid_markup) {
    const std::string svg = render_svg("flowchart LR\nA[(Store)]");
    ASSERT_TRUE(has(svg, "class=\"basic label-container outer-path\""));
    ASSERT_TRUE(has(svg, " 0,0,0 "));                   // arcs are written "a<rx>,<ry> 0,0,0 <dx>,0"
    ASSERT_TRUE(has(svg, "d=\"M0,"));                   // starts at x = 0
    ASSERT_TRUE(has(svg, "transform=\"translate(-60, -0.5)\""));  // label: -(21 / 2) + 15 / 1.5
}

// Mermaid keeps the styles of a node in an object: a repeated property keeps its first position but
// takes the last value (userNodeOverrides / styles2String).
UTEST(render, repeated_style_properties_collapse_to_the_last_value) {
    const std::string svg = render_svg(
        "flowchart TD\nclassDef a fill:#111,stroke:#222\nclassDef b fill:#333,stroke-width:2px\nA:::a --> B\nclass A b");
    ASSERT_TRUE(has(svg, "style=\"fill:#333 !important;stroke:#222 !important;stroke-width:2px !important\""));
}

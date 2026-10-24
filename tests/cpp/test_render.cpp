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

// Reference: Mermaid 12.2.0 (dagre) cluster of stress_flowchart_comments_and_directives_010. The box is the dagre
// compound node, so it is wider than its children (edge-label dummies of `ok -->|yes| ...` belong to it).
// (The label is 1/64 px narrower than in the reference, 50.59375: Chromium rounds layout units; parity tolerance is 0.5.)
UTEST(render, cluster_is_the_dagre_compound_box_in_neo_markup) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg(
        "flowchart TD\nsubgraph S[\"Settings\"]\n cfg[\"config.json\"] --> parse[\"parse()\"]\n parse --> ok{ok?}\nend\n"
        "ok -->|yes| out[\"write output\"]\nok -->|no| err[\"raise error\"]\n",
        o);
    ASSERT_TRUE(has(svg,
        "<g class=\"clusters\"><g class=\"cluster\" id=\"t-S\" data-look=\"neo\" data-color-id=\"color-0\">"
        "<rect style=\"\" x=\"8\" y=\"8\" width=\"352.5\" height=\"411\"/>"
        "<g class=\"cluster-label\" transform=\"translate(158.9609375, 8)\">"
        "<foreignObject width=\"50.578125\" height=\"21\">"
        "<div xmlns=\"http://www.w3.org/1999/xhtml\" style=\"display: table-cell; white-space: nowrap; line-height: 1.5;\">"
        "<span class=\"nodeLabel\"><p>Settings</p></span></div></foreignObject></g></g></g>"));
}

UTEST(render, cluster_colour_slots_follow_declaration_order_depth_first) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg(
        "flowchart TD\nsubgraph A\n subgraph B\n  b1\n end\n a1\nend\nsubgraph C\n c1\nend\n", o);
    ASSERT_TRUE(has(svg, "id=\"t-A\" data-look=\"neo\" data-color-id=\"color-0\""));
    ASSERT_TRUE(has(svg, "id=\"t-B\" data-look=\"neo\" data-color-id=\"color-1\""));
    ASSERT_TRUE(has(svg, "id=\"t-C\" data-look=\"neo\" data-color-id=\"color-2\""));
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

// shapes/stadium.ts draws a rough.js path (roughness 0, solid fill, seed 1): a fill path and a stroke path made
// of cubic segments whose control points come from rough.js's seeded generator. Numbers from
// tests/data/mermaid_js_dagre/stress_flowchart_escape_sequences_and_quotes_012.svg.
UTEST(render, stadium_is_a_rough_js_path_with_the_seeded_control_points) {
    const std::string svg = render_svg("flowchart LR\nA([end])");
    ASSERT_TRUE(has(svg, "<g class=\"basic label-container outer-path\"><path d=\"M-63.125 -22.5 C-29.942233963520266 -22.5, 3.2405320729594678 -22.5, 63.125 -22.5 C63.125 -22.5, 63.125 -22.5, 63.125 -22.5"));
    ASSERT_TRUE(has(svg, "stroke=\"none\" stroke-width=\"0\" fill=\"#ffffff\" style=\"\"/>"));
    ASSERT_TRUE(has(svg, "<path d=\"M-63.125 -22.5 C-37.87443243211601 -22.5, -12.623864864232019 -22.5, 63.125 -22.5 M-63.125 -22.5 C-35.17932365403976 -22"));
    ASSERT_TRUE(has(svg, "stroke=\"#28253D\" stroke-width=\"1.3\" fill=\"none\" stroke-dasharray=\"0 0\" style=\"\"/>"));
}

UTEST(render, stadium_takes_fill_stroke_and_width_from_its_styles) {
    const std::string svg = render_svg("flowchart LR\nA([end])\nstyle A fill:#f00,stroke:#00f,stroke-width:3px");
    ASSERT_TRUE(has(svg, "stroke=\"none\" stroke-width=\"0\" fill=\"#f00\""));
    ASSERT_TRUE(has(svg, "stroke=\"#00f\" stroke-width=\"3\" fill=\"none\""));
}

UTEST(render, init_theme_variables_reach_the_stylesheet) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg(
        "%%{init: {\"themeVariables\": {\"fontFamily\": \"arial\", \"clusterBkg\": \"#f8f8f8\", \"clusterBorder\": \"#999999\"}}}%%\n"
        "flowchart TD\nsubgraph S\n a-->b\nend\n", o);
    ASSERT_TRUE(has(svg, "#t{font-family:arial;font-size:14px;"));
    ASSERT_TRUE(has(svg, "cluster rect{fill:#f8f8f8;stroke:#999999;stroke-width:1px;}"));
}

UTEST(render, closed_clusters_are_nested_root_groups) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg(
        "flowchart TB\nsubgraph ClusterA\n  direction LR\n  a1[One] --> a2[Two]\nend\n"
        "subgraph ClusterB\n  direction LR\n  b1[Three] --> b2[Four]\nend\nClusterA --> ClusterB\n", o);
    ASSERT_TRUE(has(svg, "<g class=\"nodes\"><g class=\"root\" transform=\"translate(0, "));
    ASSERT_TRUE(has(svg, "<g class=\"clusters\"></g><g class=\"edgePaths\"><path d=\"M"));  // the outer root has no clusters of its own
    ASSERT_TRUE(has(svg, "id=\"t-L_ClusterA_ClusterB_0\""));
    ASSERT_TRUE(has(svg, "id=\"t-L_a1_a2_0\""));
    ASSERT_TRUE(svg.find("t-ClusterB") < svg.find("t-ClusterA"));  // B first, like Mermaid
}

UTEST(render, markdown_node_labels_get_their_class) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg("flowchart LR\nA@{ label: \"A: (Edge Text)\" }\nB[\"plain\"]\nA --> B\n", o);
    ASSERT_TRUE(has(svg, "<span class=\"nodeLabel markdown-node-label\"><p>A: (Edge Text)</p></span>"));
    ASSERT_TRUE(has(svg, "<span class=\"nodeLabel\"><p>plain</p></span>"));
}

// Mermaid inserts the classDef rules into a browser CSSStyleSheet, so the values come back the way Chromium
// serialises them: lists with commas, multi-word font names quoted.
UTEST(render, class_def_values_are_serialised_like_the_browser_css_om) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg(
        "flowchart LR\n classDef a stroke-dasharray: 5 5\n classDef b stroke-dasharray:5   5   2\n"
        " classDef c stroke-dasharray: 3,4\n classDef d font-family: Arial Black\n classDef e font-family: serif\n"
        " x:::a --> y:::b\n z:::c\n w:::d\n v:::e\n", o);
    ASSERT_TRUE(has(svg, ".a span{stroke-dasharray:5,5!important;}"));
    ASSERT_TRUE(has(svg, ".b span{stroke-dasharray:5,5,2!important;}"));
    // a comma ends the declaration (flowDb.addClass); the stray `4` is no declaration and the CSSOM drops it
    ASSERT_TRUE(has(svg, ".c span{stroke-dasharray:3!important;}"));
    ASSERT_TRUE(has(svg, ".d span{font-family:\"Arial Black\"!important;}"));
    ASSERT_TRUE(has(svg, ".e span{font-family:serif!important;}"));
}

UTEST(render, self_loops_draw_helper_nodes_and_their_paths_after_the_other_edges) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg("flowchart TD\n a[A] --> a\n a --> b[B]\n", o);
    // the helpers are drawn after the real nodes, without the svg id prefix
    ASSERT_TRUE(has(svg, "<g class=\"label edgeLabel\" id=\"a---a---1\" transform=\"translate("));
    ASSERT_TRUE(has(svg, "<rect width=\"0.1\" height=\"0.1\"></rect>") || has(svg, "<rect width=\"0.1\" height=\"0.1\"/>"));
    ASSERT_TRUE(has(svg, "style=\"display: table-cell; white-space: nowrap; line-height: 1.5; max-width: 10px; text-align: center;\""));
    // the ordinary edge is drawn before the loop
    ASSERT_TRUE(svg.find("id=\"t-L_a_b_0\"") < svg.find("id=\"t-L_a_a_0\""));
    // while the edge labels keep the order of the graph: the loop's comes first
    ASSERT_TRUE(svg.find("data-id=\"L_a_a_0\" transform") < svg.find("data-id=\"L_a_b_0\" transform"));
}

// `a ~~~ b` is drawn: an invisible path (thickness class `invisible`, no markers) and an empty label, which still take
// part in the layout like any other edge.
UTEST(render, invisible_edges_are_drawn_as_invisible_paths) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg("flowchart LR\n a ~~~ b\n b --> c\n", o);
    ASSERT_TRUE(has(svg, "id=\"t-L_a_b_0\" class=\"edge-thickness-invisible edge-pattern-solid\""));
    ASSERT_TRUE(has(svg, "style=\"stroke-dasharray: 0 0 50 0; stroke-dashoffset: 0;;\""));
    ASSERT_TRUE(has(svg, "data-id=\"L_a_b_0\""));
    const size_t path = svg.find("id=\"t-L_a_b_0\"");
    const size_t end = svg.find("/>", path);
    ASSERT_TRUE(svg.substr(path, end - path).find("marker-") == std::string::npos);
    // its empty label is there too: three labels for two edges plus the invisible one
    size_t labels = 0;
    for (size_t at = svg.find("<g class=\"edgeLabel\""); at != std::string::npos; at = svg.find("<g class=\"edgeLabel\"", at + 1)) ++labels;
    ASSERT_EQ(2u, labels);
}

UTEST(render, wrapped_edge_labels_are_a_table_of_the_wrap_width) {
    RenderOptions o;
    o.svg_id = "t";
    const std::string svg = render_svg(
        "flowchart TD\n a -->|a very long edge label that will certainly wrap because it is wider than two hundred pixels for sure| b\n", o);
    ASSERT_TRUE(has(svg, "<foreignObject width=\"200\" height=\"84\">"));
    ASSERT_TRUE(has(svg, "style=\"display: table; white-space: break-spaces; line-height: 1.5; max-width: 200px; text-align: center; width: 200px;\""));
}

UTEST(render, link_style_colour_reaches_the_edge_label_div_and_span) {
    const std::string svg = render_svg("flowchart LR\nA -->|hi| B\nA --> C\nlinkStyle 0 color:red");
    ASSERT_TRUE(has(svg, "style=\"color: red !important; display: table-cell;"));
    ASSERT_TRUE(has(svg, "<span style=\"color:red !important\" class=\"edgeLabel\">"));
}

UTEST(render, class_def_reaches_the_cluster_class_and_rect) {
    const std::string svg = render_svg(
        "flowchart TB\nclassDef hot fill:#f96,stroke:#333\nsubgraph s1[Group]\na --> b\nend\nclass s1 hot\n");
    ASSERT_TRUE(has(svg, "<g class=\"cluster hot\" id="));
    ASSERT_TRUE(has(svg, "<rect style=\"fill:#f96 !important;stroke:#333 !important\""));
}

UTEST(render, of_several_loops_on_one_node_only_the_last_is_drawn) {
    // dagre's prepareLayoutForDagre names every loop's helpers after the node, so a later loop replaces an earlier one
    const std::string svg = render_svg("flowchart LR\nA --> A\nA --> A\nA --> B");
    ASSERT_FALSE(has(svg, "data-id=\"L_A_A_0\" data-points"));
    ASSERT_TRUE(has(svg, "data-id=\"L_A_A_2\" data-points"));
}

UTEST(render, classic_themes_use_their_own_variables_font_and_line_height) {
    for (const char* theme : {"dark", "forest", "neutral", "base"}) {
        const std::string svg = render_svg(std::string("%%{init: {\"theme\": \"") + theme + "\"}}%%\nflowchart TD\nA-->B");
        ASSERT_TRUE(has(svg, "font-family:\"trebuchet ms\",verdana,arial,sans-serif;font-size:16px;"));
        ASSERT_TRUE(has(svg, "height=\"24\""));  // 16px text: 24px lines
        ASSERT_FALSE(has(svg, "data-color-id"));  // only redux-color has colour slots
        ASSERT_TRUE(has(svg, "<linearGradient id="));
    }
    const std::string dark = render_svg("%%{init: {\"theme\": \"dark\"}}%%\nflowchart TD\nA-->B");
    ASSERT_TRUE(has(dark, "flood-color=\"#FFFFFF\""));
    ASSERT_TRUE(has(dark, "fill:#ccc;"));
}

UTEST(render, theme_option_and_init_directive_draw_the_same_diagram) {
    for (const char* theme : {"dark", "forest", "neutral", "base"}) {
        RenderOptions o;
        o.theme = theme;
        const std::string by_option = render_svg("flowchart TD\nA-->B", o);
        const std::string by_init = render_svg(std::string("%%{init: {\"theme\": \"") + theme + "\"}}%%\nflowchart TD\nA-->B");
        ASSERT_TRUE(by_option == by_init);
    }
}

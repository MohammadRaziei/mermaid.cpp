// Compares mermaid::render_svg with real Mermaid 12.2.0 SVGs (layout: dagre) in tests/data/mermaid_js_dagre.
#include <fstream>
#include <iostream>
#include <sstream>

#include <mermaid/render.h>

#include "svg_compare.h"
#include "utest.h"

namespace {

std::string read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const std::string kData = MERMAID_TEST_DATA_DIR;

}  // namespace

UTEST(svg_parity, fixtures_marked_pass_match_mermaid_js_and_todo_ones_report_progress) {
    std::ifstream list(kData + "/mermaid_js_dagre/parity.tsv");
    ASSERT_TRUE(list.good());
    std::string line;
    while (std::getline(list, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string name = line.substr(0, tab);
        std::string status = line.substr(tab + 1);
        while (!status.empty() && (status.back() == '\r' || status.back() == ' ')) status.pop_back();

        mermaid::RenderOptions options;
        options.svg_id = name == "basic" ? "basic" : name;
        const std::string actual = mermaid::render_svg(read_all(kData + "/merman/" + name + ".mmd"), options);
        const auto diffs = svgtest::svg_comparer(0.5).compare(read_all(kData + "/mermaid_js_dagre/" + name + ".svg"), actual);

        if (status == "pass") {
            for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << name << ": " << diffs[i] << "\n";
            EXPECT_EQ(0u, diffs.size());
        } else {
            std::cout << "[ parity ] " << name << ": " << diffs.size() << " differences (todo)\n";
            for (size_t i = 0; i < diffs.size() && i < 12; ++i) std::cout << "    " << diffs[i] << "\n";
        }
    }
}

// The frame of the document: the <svg> element, the marker group, the filter definitions. The
// <style> text and the content of <g class="root"> are covered by later steps.
UTEST(svg_parity, svg_frame_matches_mermaid_js_outside_style_and_root_content) {
    // Fixtures left out: 003 (its height depends on the ELK layout), 034 (linkStyle colours add
    // coloured marker copies).
    for (const char* name : {"basic"}) {
        mermaid::RenderOptions options;
        options.svg_id = name;
        const std::string actual = mermaid::render_svg(read_all(kData + "/merman/" + name + ".mmd"), options);
        svgtest::svg_comparer cmp(0.5);
        cmp.prune("style").prune("g.root");
        const auto diffs = cmp.compare(read_all(kData + "/mermaid_js_dagre/" + name + ".svg"), actual);
        for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << name << ": " << diffs[i] << "\n";
        EXPECT_EQ(0u, diffs.size());
    }
}

// The node group: shapes (box, diamond), label markup, DOM ids, transforms.
UTEST(svg_parity, nodes_of_basic_match_mermaid_js) {
    mermaid::RenderOptions options;
    options.svg_id = "basic";
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/basic.mmd"), options);
    const auto diffs = svgtest::svg_comparer(0.5).compare_within("g.nodes", read_all(kData + "/mermaid_js_dagre/basic.svg"), actual);
    for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());
}

// The stylesheet text (theme variables, flowchart styles, neo look rules, classDef rules).
UTEST(svg_parity, stylesheet_of_basic_matches_mermaid_js) {
    mermaid::RenderOptions options;
    options.svg_id = "basic";
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/basic.mmd"), options);
    const auto diffs = svgtest::svg_comparer(0.5).compare_within("style", read_all(kData + "/mermaid_js_dagre/basic.svg"), actual);
    for (size_t i = 0; i < diffs.size() && i < 5; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());
}

UTEST(svg_parity, stylesheet_with_class_defs_matches_mermaid_js) {
    const std::string name = "stress_flowchart_classdef_and_inline_classes_003";
    mermaid::RenderOptions options;
    options.svg_id = name;
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/" + name + ".mmd"), options);
    const auto diffs = svgtest::svg_comparer(0.5).compare_within("style", read_all(kData + "/mermaid_js_dagre/" + name + ".svg"), actual);
    for (size_t i = 0; i < diffs.size() && i < 5; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());
}

// The edge group of `basic`: ids, classes, data-* attributes, markers. Geometry (`d`,
// `data-points`) and the dash lengths in `style` depend on the layout engine (the reference was
// laid out by ELK, we use dagre), so only their presence is compared here; the geometry itself is
// covered by test_edge_geometry.
UTEST(svg_parity, edge_paths_of_basic_match_mermaid_js_markup) {
    mermaid::RenderOptions options;
    options.svg_id = "basic";
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/basic.mmd"), options);
    svgtest::svg_comparer cmp(0.5);
    cmp.ignore_attribute("d").ignore_attribute("data-points").ignore_attribute("style");
    const auto diffs = cmp.compare_within("g.edgePaths", read_all(kData + "/mermaid_js_dagre/basic.svg"), actual);
    for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());
}

// The edge labels of `basic`. The label width (and so `width` and the centring `transform`)
// comes from the browser's font metrics, which ApproxTextMeasurer does not reproduce.
UTEST(svg_parity, edge_labels_of_basic_match_mermaid_js_markup) {
    mermaid::RenderOptions options;
    options.svg_id = "basic";
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/basic.mmd"), options);
    svgtest::svg_comparer cmp(0.5);
    cmp.ignore_attribute("width").ignore_attribute("transform");
    const auto diffs = cmp.compare_within("g.edgeLabels", read_all(kData + "/mermaid_js_dagre/basic.svg"), actual);
    for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());
}

// linkStyle colours: markup, style text and coloured marker copies (after <g class="root">).
UTEST(svg_parity, edge_paths_and_markers_with_link_style_match_mermaid_js_markup) {
    const std::string name = "stress_flowchart_amp_groups_edge_ids_034";
    mermaid::RenderOptions options;
    options.svg_id = name;
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/" + name + ".mmd"), options);
    const std::string expected = read_all(kData + "/mermaid_js_dagre/" + name + ".svg");
    svgtest::svg_comparer paths(0.5);
    // the dash lengths in `style` differ with the route; the style text is checked in test_render
    paths.ignore_attribute("d").ignore_attribute("data-points").ignore_attribute("style");
    const auto diffs = paths.compare_within("g.edgePaths", expected, actual);
    for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());

    // 12 markers + root + 3 coloured copies = 16 children of the marker group
    svgtest::svg_comparer markers(0.5);
    markers.prune("g.root").prune("style");
    const auto mdiffs = markers.compare(expected, actual);
    for (size_t i = 0; i < mdiffs.size() && i < 10; ++i) std::cout << "  " << mdiffs[i] << "\n";
    EXPECT_EQ(0u, mdiffs.size());
}

// ---- Reference: real Mermaid 12.2.0 with `layout: dagre` (tests/data/mermaid_js_dagre) ----

namespace {

std::vector<std::string> compare_dagre(const std::string& name, const std::string& selector, svgtest::svg_comparer cmp) {
    mermaid::RenderOptions options;
    options.svg_id = name;
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/" + name + ".mmd"), options);
    const std::string expected = read_all(kData + "/mermaid_js_dagre/" + name + ".svg");
    return selector.empty() ? cmp.compare(expected, actual) : cmp.compare_within(selector, expected, actual);
}

void print_diffs(const std::vector<std::string>& diffs) {
    for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << diffs[i] << "\n";
}

}  // namespace

UTEST(svg_parity_dagre, nodes_and_viewbox_of_basic_match) {
    const auto nodes = compare_dagre("basic", "g.nodes", svgtest::svg_comparer(0.5));
    print_diffs(nodes);
    EXPECT_EQ(0u, nodes.size());
    svgtest::svg_comparer frame(0.5);
    frame.prune("style").prune("g.root");
    const auto diffs = compare_dagre("basic", "", frame);
    print_diffs(diffs);
    EXPECT_EQ(0u, diffs.size());
}

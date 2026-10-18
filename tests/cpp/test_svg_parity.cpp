// Compares mermaid::render_svg with the real Mermaid SVGs in tests/data/mermaid_js.
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
    std::ifstream list(kData + "/mermaid_js/parity.tsv");
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
        const auto diffs = svgtest::SvgComparer(0.5).compare(read_all(kData + "/mermaid_js/" + name + ".svg"), actual);

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
        svgtest::SvgComparer cmp(0.5);
        cmp.prune("style").prune("g.root");
        const auto diffs = cmp.compare(read_all(kData + "/mermaid_js/" + name + ".svg"), actual);
        for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << name << ": " << diffs[i] << "\n";
        EXPECT_EQ(0u, diffs.size());
    }
}

// The node group: shapes (box, diamond), label markup, DOM ids, transforms.
UTEST(svg_parity, nodes_of_basic_match_mermaid_js) {
    mermaid::RenderOptions options;
    options.svg_id = "basic";
    const std::string actual = mermaid::render_svg(read_all(kData + "/merman/basic.mmd"), options);
    const auto diffs = svgtest::SvgComparer(0.5).compare_within("g.nodes", read_all(kData + "/mermaid_js/basic.svg"), actual);
    for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << diffs[i] << "\n";
    EXPECT_EQ(0u, diffs.size());
}

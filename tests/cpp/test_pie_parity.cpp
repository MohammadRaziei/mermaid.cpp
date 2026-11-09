// Compares mermaid::render_svg of pie charts with real Mermaid 12.2.0 SVGs (tests/data/pie/*.mmd; the
// status of every fixture is in tests/data/pie/parity.tsv: `pass` must match, `todo` only reports progress).
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

const std::string kData = std::string(MERMAID_TEST_DATA_DIR) + "/pie/";

}  // namespace

UTEST(pie_parity, fixtures_marked_pass_match_mermaid_js_and_todo_ones_report_progress) {
    const std::string refs = MERMAID_REFERENCE_DIR;
    if (!std::ifstream(refs + "/pie_basic.svg").good()) {
        std::cout << "[ skipped ] no pie reference SVGs\n";
        return;
    }
    std::ifstream list(kData + "parity.tsv");
    ASSERT_TRUE(list.good());
    std::string line;
    while (std::getline(list, line)) {
        const size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        const std::string name = line.substr(0, tab);
        std::string status = line.substr(tab + 1);
        while (!status.empty() && (status.back() == '\r' || status.back() == ' ')) status.pop_back();
        mermaid::RenderOptions options;
        options.svg_id = name;
        std::string actual;
        try {
            actual = mermaid::render_svg(read_all(kData + name + ".mmd"), options);
        } catch (const std::exception& e) {
            EXPECT_TRUE(status != "pass");
            std::cout << "[ parity ] " << name << ": not rendered (todo): " << e.what() << "\n";
            continue;
        }
        const auto diffs = svgtest::svg_comparer(0.5).compare(read_all(refs + "/" + name + ".svg"), actual);
        if (status == "pass") {
            for (size_t i = 0; i < diffs.size() && i < 10; ++i) std::cout << "  " << name << ": " << diffs[i] << "\n";
            EXPECT_EQ(0u, diffs.size());
        } else {
            std::cout << "[ parity ] " << name << ": " << diffs.size() << " differences (todo)\n";
            for (size_t i = 0; i < diffs.size() && i < 12; ++i) std::cout << "    " << diffs[i] << "\n";
        }
    }
}

#include "doctest.h"
#include "mermaid.h"
#include "common.h"

#include <algorithm>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace {
const fs::path kRootDir = fs::path(__FILE__).parent_path().parent_path();
const fs::path kDataDir = kRootDir / "tests" / "data" / "sequence_diagram";
}

TEST_CASE("sequence diagrams match golden SVG outputs") {
    REQUIRE(fs::exists(kDataDir));

    std::vector<fs::path> mermaid_files;
    for (auto &entry : fs::directory_iterator(kDataDir)) {
        if (entry.path().extension() == ".mermaid") {
            mermaid_files.push_back(entry.path());
        }
    }

    std::sort(mermaid_files.begin(), mermaid_files.end());

    for (const auto &mermaid_path : mermaid_files) {
        fs::path svg_path = mermaid_path;
        svg_path.replace_extension(".svg");

        CAPTURE(mermaid_path.string());
        CAPTURE(svg_path.string());
        REQUIRE(fs::exists(svg_path));

        std::string source = test_utils::load_file_required(mermaid_path);
        std::string expected_svg = test_utils::load_file_required(svg_path);
        std::string actual_svg = mermaid::render_to_svg(source);

        CHECK(test_utils::normalize(actual_svg) == test_utils::normalize(expected_svg));
    }
}

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "mermaid.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

std::string load_file(const fs::path &p) {
    std::ifstream ifs(p);
    REQUIRE(ifs.good());
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

std::string normalize(const std::string &input) {
    std::string filtered;
    filtered.reserve(input.size());
    for (char c : input) {
        if (c == '\r') {
            continue;
        }
        filtered.push_back(c);
    }

    auto trim = [](std::string &s) {
        while (!s.empty() && (s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
            s.pop_back();
        }
        std::size_t start = 0;
        while (start < s.size() && (s[start] == '\n' || s[start] == ' ' || s[start] == '\t')) {
            start++;
        }
        if (start > 0) {
            s.erase(0, start);
        }
    };

    trim(filtered);
    return filtered;
}

TEST_CASE("sequence diagrams match golden SVG outputs") {
    fs::path this_file = __FILE__;
    fs::path tests_dir = this_file.parent_path();
    fs::path data_dir = tests_dir / "data";

    REQUIRE(fs::exists(data_dir));

    for (auto &entry : fs::directory_iterator(data_dir)) {
        if (entry.path().extension() == ".mermaid") {
            fs::path mermaid_path = entry.path();
            fs::path svg_path = mermaid_path;
            svg_path.replace_extension(".svg");

            CAPTURE(mermaid_path.string());
            CAPTURE(svg_path.string());
            REQUIRE(fs::exists(svg_path));

            std::string source = load_file(mermaid_path);
            std::string expected_svg = load_file(svg_path);
            std::string actual_svg = mermaid::render_to_svg(source);

            CHECK(normalize(actual_svg) == normalize(expected_svg));
        }
    }
}


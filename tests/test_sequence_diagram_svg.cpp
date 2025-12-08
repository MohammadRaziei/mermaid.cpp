#include "doctest.h"
#include "mermaid.h"
#include "common.h"

#include <algorithm>
#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

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

        bool isequal = test_utils::normalize(actual_svg) == test_utils::normalize(expected_svg);
        if (!isequal) {
            std::cerr << "Mismatch for " << mermaid_path.filename().string() << std::endl;
            std::cerr << "Generated SVG (first 2000 chars):\n" << actual_svg.substr(0, 2000) << std::endl;
            std::cerr << "Expected SVG (first 2000 chars):\n" << expected_svg.substr(0, 2000) << std::endl;
        }
        CHECK(isequal);
    }
}

TEST_CASE("debug blocks_par_alt") {
    fs::path mermaid_path = kDataDir / "blocks_par_alt.mermaid";
    fs::path svg_path = mermaid_path;
    svg_path.replace_extension(".svg");
    REQUIRE(fs::exists(mermaid_path));
    REQUIRE(fs::exists(svg_path));
    std::string source = test_utils::load_file_required(mermaid_path);
    std::string expected_svg = test_utils::load_file_required(svg_path);
    std::string actual_svg = mermaid::render_to_svg(source);
    // Write to files for inspection
    std::ofstream out_gen("/tmp/gen.svg");
    out_gen << actual_svg;
    out_gen.close();
    std::ofstream out_exp("/tmp/exp.svg");
    out_exp << expected_svg;
    out_exp.close();
    // Compare line by line
    std::istringstream gen_stream(actual_svg);
    std::istringstream exp_stream(expected_svg);
    std::string gen_line, exp_line;
    int line_num = 1;
    while (std::getline(gen_stream, gen_line) && std::getline(exp_stream, exp_line)) {
        if (gen_line != exp_line) {
            std::cerr << "Difference at line " << line_num << std::endl;
            std::cerr << "Generated: " << gen_line << std::endl;
            std::cerr << "Expected:  " << exp_line << std::endl;
            break;
        }
        line_num++;
    }
    CHECK(actual_svg == expected_svg);
}

TEST_CASE("debug three_participants") {
    fs::path mermaid_path = kDataDir / "three_participants.mermaid";
    fs::path svg_path = mermaid_path;
    svg_path.replace_extension(".svg");
    REQUIRE(fs::exists(mermaid_path));
    REQUIRE(fs::exists(svg_path));
    std::string source = test_utils::load_file_required(mermaid_path);
    std::string expected_svg = test_utils::load_file_required(svg_path);
    std::string actual_svg = mermaid::render_to_svg(source);
    // Write to files for inspection
    std::ofstream out_gen("/tmp/gen3.svg");
    out_gen << actual_svg;
    out_gen.close();
    std::ofstream out_exp("/tmp/exp3.svg");
    out_exp << expected_svg;
    out_exp.close();
    // Compare line by line
    std::istringstream gen_stream(actual_svg);
    std::istringstream exp_stream(expected_svg);
    std::string gen_line, exp_line;
    int line_num = 1;
    while (std::getline(gen_stream, gen_line) && std::getline(exp_stream, exp_line)) {
        if (gen_line != exp_line) {
            std::cerr << "Difference at line " << line_num << std::endl;
            std::cerr << "Generated: " << gen_line << std::endl;
            std::cerr << "Expected:  " << exp_line << std::endl;
            break;
        }
        line_num++;
    }
    CHECK(actual_svg == expected_svg);
}

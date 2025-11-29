#include "mermaid.h"
#include "CLI11.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string read_file(const std::filesystem::path &path) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Failed to open input file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    return buffer.str();
}

void write_file(const std::filesystem::path &path, const std::string &contents) {
    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }
    ofs << contents;
}

std::filesystem::path derive_output_path(const std::filesystem::path &input_path) {
    std::filesystem::path output = input_path;
    output.replace_extension(".svg");
    return output;
}

} // namespace

int main(int argc, char **argv) {
    CLI::App app{"Native Mermaid sequence diagram renderer"};

    std::string input_path_str;
    std::string output_path_str;

    app.add_option("input", input_path_str, "Input Mermaid file")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-o,--output", output_path_str, "Output SVG file");

    try {
        CLI11_PARSE(app, argc, argv);

        std::filesystem::path input_path{input_path_str};
        std::filesystem::path output_path = output_path_str.empty()
                                             ? derive_output_path(input_path)
                                             : std::filesystem::path{output_path_str};

        std::string source = read_file(input_path);
        std::string svg = mermaid::render_to_svg(source);
        write_file(output_path, svg);

        std::cout << "Wrote SVG to " << output_path << std::endl;
        return 0;
    } catch (const CLI::ParseError &e) {
        return app.exit(e);
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

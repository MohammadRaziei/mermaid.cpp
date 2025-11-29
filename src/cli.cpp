#include "mermaid.h"

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

void print_usage(const std::string &program_name) {
    std::cerr << "Usage: " << program_name << " <diagram.mermaid>" << std::endl;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    try {
        std::filesystem::path input_path{argv[1]};
        if (!std::filesystem::exists(input_path)) {
            std::cerr << "Input file does not exist: " << input_path << std::endl;
            return 1;
        }

        std::string source = read_file(input_path);
        std::string svg = mermaid::render_to_svg(source);

        std::filesystem::path output_path = derive_output_path(input_path);
        write_file(output_path, svg);

        std::cout << "Wrote SVG to " << output_path << std::endl;
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}

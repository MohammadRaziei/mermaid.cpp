#include "mermaid.h"
#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::ifstream file("../tests/data/sequence_diagram/loop_basic.mermaid");
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    try {
        std::string svg = mermaid::render_to_svg(source);
        std::cout << "SVG generated successfully, length: " << svg.length() << std::endl;
        // Write to file for inspection
        std::ofstream out("loop_output.svg");
        out << svg;
        out.close();
        std::cout << "Written to loop_output.svg\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

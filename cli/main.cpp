// mermaid_cli - renders a Mermaid diagram to SVG. See mermaid/cli.h.
#include <iostream>
#include <string>
#include <vector>

#include <mermaid/cli.h>

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    return mermaid::cli::run(args, std::cin, std::cout, std::cerr);
}

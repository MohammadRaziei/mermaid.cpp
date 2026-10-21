// A minimal use of the library: Mermaid text in, SVG text out.
#include <iostream>

#include <mermaid/error.h>
#include <mermaid/render.h>

int main() {
    try {
        mermaid::RenderOptions options;
        options.theme = "forest";
        std::cout << mermaid::render_svg("flowchart LR\n  A[Start] --> B{Ok?}\n  B -->|yes| C[Done]\n  B -->|no| A\n", options);
    } catch (const mermaid::ParseError& e) {
        std::cerr << "line " << e.line() << ": " << e.what() << "\n";
        return 1;
    }
    return 0;
}

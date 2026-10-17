#include "style_css.h"

namespace mermaid::detail {

std::string shape_declarations(const flowchart::Style& style) {
    std::string out;
    for (const auto& [key, value] : style) {
        if (key == "color" || key.compare(0, 5, "font-") == 0) continue;
        out += key + ":" + value + ";";
    }
    return out;
}

std::string text_declarations(const flowchart::Style& style) {
    std::string out;
    for (const auto& [key, value] : style) {
        if (key == "color") out += "fill:" + value + ";";
        else if (key.compare(0, 5, "font-") == 0) out += key + ":" + value + ";";
    }
    return out;
}

}  // namespace mermaid::detail

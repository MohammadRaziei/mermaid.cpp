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

std::string important_shape_style(const flowchart::Style& style) {
    std::string out;
    for (const auto& [key, value] : style) {
        if (key == "color" || key.compare(0, 5, "font-") == 0) continue;
        if (!out.empty()) out += ";";
        out += key + ":" + value + " !important";
    }
    return out;
}

namespace {
const std::string* color_of(const flowchart::Style& style) {
    for (const auto& [key, value] : style)
        if (key == "color") return &value;
    return nullptr;
}
}  // namespace

std::string important_label_style(const flowchart::Style& style) {
    const std::string* c = color_of(style);
    return c ? "color:" + *c + " !important" : std::string();
}

std::string computed_label_color(const flowchart::Style& style) {
    const std::string* c = color_of(style);
    if (!c) return {};
    auto hex = [](char ch) { return ch >= '0' && ch <= '9' ? ch - '0' : (ch | 32) - 'a' + 10; };
    auto is_hex = [](char ch) { return (ch >= '0' && ch <= '9') || ((ch | 32) >= 'a' && (ch | 32) <= 'f'); };
    std::string v = *c;
    if (v.size() == 4 && v[0] == '#') v = std::string("#") + v[1] + v[1] + v[2] + v[2] + v[3] + v[3];
    std::string shown = v;
    if (v.size() == 7 && v[0] == '#') {
        bool ok = true;
        for (size_t i = 1; i < 7; ++i) ok = ok && is_hex(v[i]);
        if (ok)
            shown = "rgb(" + std::to_string(hex(v[1]) * 16 + hex(v[2])) + ", " + std::to_string(hex(v[3]) * 16 + hex(v[4])) +
                    ", " + std::to_string(hex(v[5]) * 16 + hex(v[6])) + ")";
    }
    return "color: " + shown + " !important; ";
}

}  // namespace mermaid::detail

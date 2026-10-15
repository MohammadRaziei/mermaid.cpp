// text_measurer.h - abstraction over "how wide is this text?".
//
// Font metrics are the one thing that differs per platform, so every stage that
// needs text size depends on this interface, never on a concrete font engine.
#pragma once

#include <string_view>

#include "geometry.h"

namespace mermaidcpp {

struct TextStyle {
    double font_size = 14.0;       // px (Mermaid 11.16 flowchart default)
    double line_height_em = 1.5;   // 14px * 1.5 = 21px, as in upstream SVG labels

    double line_height() const { return font_size * line_height_em; }
};

class TextMeasurer {
public:
    explicit TextMeasurer(TextStyle style = {}) : style_(style) {}
    virtual ~TextMeasurer() = default;
    TextMeasurer(const TextMeasurer&) = delete;
    TextMeasurer& operator=(const TextMeasurer&) = delete;

    // Width in px of one line of UTF-8 text (no '\n' inside).
    virtual double line_width(std::string_view line) const = 0;

    // Box of possibly multi-line text: widest line x (lines * line height).
    Size measure(std::string_view text) const;

    const TextStyle& style() const { return style_; }

private:
    TextStyle style_;
};

}  // namespace mermaidcpp

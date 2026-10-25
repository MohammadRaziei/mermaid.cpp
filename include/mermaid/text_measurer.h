// text_measurer.h - abstraction over "how wide is this text?".
//
// Font metrics are the one thing that differs per platform, so every stage that
// needs text size depends on this interface, never on a concrete font engine.
#pragma once

#include <string_view>

#include <mermaid/geometry.h>

namespace mermaid {

struct TextStyle {
    double font_size = 14.0;       // px (Mermaid 12.2 flowchart default)
    double line_height_em = 1.5;   // 14px * 1.5 = 21px, as in upstream SVG labels

    double line_height() const { return font_size * line_height_em; }
};

// The four faces text of a Markdown label comes in.
enum class FontFace { Regular = 0, Bold = 1, Italic = 2, BoldItalic = 3 };

class TextMeasurer {
public:
    explicit TextMeasurer(TextStyle style = {}) : style_(style) {}
    virtual ~TextMeasurer() = default;
    TextMeasurer(const TextMeasurer&) = delete;
    TextMeasurer& operator=(const TextMeasurer&) = delete;

    // Width in px of one line of UTF-8 text (no '\n' inside).
    virtual double line_width(std::string_view line) const = 0;

    // The same in a face; a measurer without per-face metrics measures every face as Regular.
    virtual double line_width(std::string_view line, FontFace) const { return line_width(line); }

    // Width of a piece of a line exactly as given: spaces are neither collapsed nor trimmed (the caller did that for
    // the whole line). The pieces of a line in several faces add up to its width.
    virtual double run_width(std::string_view run, FontFace face) const { return line_width(run, face); }

    // Box of possibly multi-line text: widest line x (lines * line height).
    Size measure(std::string_view text) const;

    // The text laid out in a box `max_width` wide, the way a browser breaks lines: at spaces, greedily, a word that
    // is wider than the box stays whole on its own line. Explicit '\n' always breaks. `wrapped` tells that some
    // line had to break; the box is then `max_width` wide (the label's div is set to that width), otherwise it is
    // the plain measurement.
    struct Wrapped {
        Size size;
        bool wrapped = false;
    };
    Wrapped measure_wrapped(std::string_view text, double max_width) const;

    const TextStyle& style() const { return style_; }

private:
    TextStyle style_;
};

}  // namespace mermaid

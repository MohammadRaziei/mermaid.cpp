// browser_text_measurer.h - zero-dependency measurer that reproduces a browser's text widths.
#pragma once

#include <mermaid/text_measurer.h>

namespace mermaid {

// Advance widths and kerning pairs (Regular, Bold, Italic and BoldItalic) of the font Mermaid's labels get in headless Chromium
// (font-family "Recursive Variable", arial, sans-serif; 14px), captured into a table by
// tools/dump_font_metrics.mjs. Widths of ASCII and Latin-1 text match the browser within
// ~0.05 px; other scripts use coarse fallbacks (CJK and other wide characters: 1 em).
class browser_text_measurer final : public TextMeasurer {
public:
    explicit browser_text_measurer(TextStyle style = {}) : TextMeasurer(style) {}
    double line_width(std::string_view line) const override { return line_width(line, FontFace::Regular); }
    double line_width(std::string_view line, FontFace face) const override;
    double run_width(std::string_view run, FontFace face) const override;
};

}  // namespace mermaid

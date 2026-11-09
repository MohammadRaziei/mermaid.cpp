// svg_text_metrics.h (private) - utils.calculateTextDimensions as Mermaid's sequence diagrams use it.
//
// The function measures the text twice, in `sans-serif` and in the configured font family, with an SVG <text> at the
// configured font size, and keeps the second unless the first is strictly bigger in width, height and line height. The
// sequence diagram's font families are empty strings in the neo look, so the second measurement is the browser default
// (serif) and decides nearly every time. Both results are rounded to whole pixels.
//
// Both faces are computed from their design units (svg_text_metrics_data.h) the way Chromium reports them: the length is
// the sum of the advances and kerning pairs rounded up to 1/64 px, the box adds the ink of the glyphs rounded outwards,
// and the height is the font's rounded ascent plus descent widened by the ink of the tallest glyphs.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mermaid::detail {

// The rows of a text: it breaks at <br>, <br/>, </br> in any case (common.splitBreaks).
std::vector<std::string> split_breaks(std::string_view text);

struct svg_text_dimensions {
    double width = 0;
    double height = 0;
};

class svg_text_metrics {
public:
    // utils.calculateTextDimensions(text, {fontSize}) with an empty font family.
    static svg_text_dimensions calculate(std::string_view text, double font_size = 16);

    // The single-font measurements, exposed for the tests.
    static svg_text_dimensions serif(std::string_view text, double font_size = 16);
    static svg_text_dimensions sans(std::string_view text, double font_size = 16);

    // The width getBoundingClientRect() reports for an SVG <text> in the stylesheet's (sans) face, not rounded: the pie chart's legend and title
    // use it.
    static double sans_bbox_width(std::string_view text, double font_size = 16);

    // The height getBBox() reports for an element drawn in the stylesheet's (sans) face: 17 at 16px, or 18 with a glyph
    // below the descent. drawText and drawNote add these up for the rows of a multi-line text.
    static double drawn_height(std::string_view text, double font_size = 16);
};

}  // namespace mermaid::detail

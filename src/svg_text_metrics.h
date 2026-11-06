// svg_text_metrics.h (private) - utils.calculateTextDimensions as Mermaid's sequence diagrams use it.
//
// The function measures the text twice, in `sans-serif` and in the configured font family, with an SVG <text> at 16px,
// and keeps the second unless the first is strictly bigger in width, height and line height. The sequence diagram's
// font families are empty strings in the neo look, so the second measurement is the browser default (serif) and decides
// nearly every time. Both results are rounded to whole pixels.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <mermaid/browser_text_measurer.h>

namespace mermaid::detail {

// The rows of a text: it breaks at <br>, <br/>, </br> in any case (common.splitBreaks).
std::vector<std::string> split_breaks(std::string_view text);

struct svg_text_dimensions {
    double width = 0;
    double height = 0;
};

class svg_text_metrics {
public:
    // `sans` measures the first family (Liberation Sans); only its width is read from it.
    explicit svg_text_metrics(const TextMeasurer& sans) : sans_(sans) {}

    svg_text_dimensions calculate(std::string_view text) const;

    // The two single-font measurements, exposed for the tests.
    static svg_text_dimensions serif(std::string_view text);

    // The height getBBox() reports for an element drawn in the stylesheet's (sans) face: 17, or 18 with a glyph below the
    // descent. drawText and drawNote add these up for the rows of a multi-line text.
    static double drawn_height(std::string_view text);

private:
    const TextMeasurer& sans_;  // at 16px
};

}  // namespace mermaid::detail

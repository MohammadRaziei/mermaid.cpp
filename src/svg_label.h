// svg_label.h (private) - a label made of SVG <text> instead of an HTML <foreignObject> (`htmlLabels: false`).
//
// createText.ts (createFormattedText) lays the words of every line out in <tspan>s and wraps lines at the label
// width with splitLineToFitWidth; the box of the label is then what the browser reports as getBBox() of the <text>.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <mermaid/geometry.h>
#include <mermaid/text_measurer.h>

#include "svg_writer.h"

namespace mermaid::detail {

struct svg_label {
    using line = std::vector<TextWord>;  // the words of one row of the text
    std::vector<line> lines;
    Size size;       // getBBox() of the <text>: widest row x (ascent + descent + the rows after the first)
    double left = 0; // bbox.x: half the widest row to the left of the origin
    double top = 0;  // bbox.y: where the first row's glyph box starts (its baseline is 1em below the <text> origin)

    // Plain text; `\n` breaks lines. `max_width` is the width the text is wrapped at (checkWidth of createFormattedText).
    static svg_label layout(const TextMeasurer& measurer, std::string_view text, double max_width);
    // The same for the lines of a Markdown label.
    static svg_label layout_words(const TextMeasurer& measurer, const std::vector<line>& paragraphs, double max_width);

    // `y` of the outer <tspan> of row `index`, as JavaScript writes `index * 1.1 - 0.1` (em).
    static std::string row_y(size_t index);

    // The <text> of createFormattedText: one outer <tspan> per row, one inner <tspan> per word (a space leads every
    // word after the first). `centered` is the text-anchor of edge labels; `style` the attribute a labelled text has.
    static void write_text(SvgWriter& out, const std::vector<line>& rows, bool centered, bool style);
};

}  // namespace mermaid::detail

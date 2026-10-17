// text_block.h (private) - centred, possibly multi-line SVG text.
#pragma once

#include <string>
#include <string_view>

#include <mermaid/geometry.h>

#include "svg_writer.h"

namespace mermaid::detail {

// One <tspan> per line ('\n' separates lines), the block centred on `center`.
void write_text_block(SvgWriter& out, const Point& center, std::string_view text, double line_height,
                      const std::string& style = {});

}  // namespace mermaid::detail

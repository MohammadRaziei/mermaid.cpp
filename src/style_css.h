// style_css.h (private) - Mermaid style lists -> inline CSS.
#pragma once

#include <string>

#include <mermaid/flowchart.h>

namespace mermaid::detail {

// "k:v;k:v;" for shapes and paths. `color` is for text, so it is left out.
std::string shape_declarations(const flowchart::Style& style);

// "fill:<color>;" plus font properties, for the label text.
std::string text_declarations(const flowchart::Style& style);

}  // namespace mermaid::detail

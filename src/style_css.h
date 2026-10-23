// style_css.h (private) - Mermaid style lists -> inline CSS.
#pragma once

#include <string>

#include <mermaid/flowchart.h>

namespace mermaid::detail {

// Mermaid's html-label node styles: "fill:#fff !important;stroke:#000 !important" (no trailing
// semicolon), `color` and font properties left out.
std::string important_shape_style(const flowchart::Style& style);

// "color:#001122 !important" for the label group and span; empty without a `color`.
std::string important_label_style(const flowchart::Style& style);

// The same colour as the browser reports it on the div: "color: rgb(0, 17, 34) !important; ".
std::string computed_label_color(const flowchart::Style& style);

}  // namespace mermaid::detail

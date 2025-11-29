#pragma once

#include <string>

namespace mermaid {

// High-level API: convert Mermaid source directly to SVG text.
// For now, only supports `sequenceDiagram` diagrams.
std::string render_to_svg(const std::string &mermaid_source);

} // namespace mermaid


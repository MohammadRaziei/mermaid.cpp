#pragma once

#include <string>

namespace mermaid {

// Render a Mermaid sequenceDiagram source into SVG text.
// Full support is expected to match mermaid.js behavior as the engine evolves.
// Throws std::runtime_error on parse or render errors.
std::string render_to_svg(const std::string &mermaid_source);

} // namespace mermaid


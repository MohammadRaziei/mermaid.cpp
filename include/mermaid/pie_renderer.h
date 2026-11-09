// pie_renderer.h - pie::Model -> SVG, drawn like Mermaid 12.2's pieRenderer.
#pragma once

#include <string>

#include <mermaid/pie.h>
#include <mermaid/render.h>

namespace mermaid::pie {

// Throws UnsupportedDiagram for settings that are not drawn yet (see checklist.md).
std::string render_svg(const Model& model, const RenderOptions& options = {});

}  // namespace mermaid::pie

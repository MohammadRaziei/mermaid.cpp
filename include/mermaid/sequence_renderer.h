// sequence_renderer.h - sequence::Model -> SVG, laid out and drawn like Mermaid 12.2's sequenceRenderer.
#pragma once

#include <string>

#include <mermaid/render.h>
#include <mermaid/sequence.h>

namespace mermaid::sequence {

// Throws UnsupportedDiagram for statements that are not drawn yet (see checklist.md).
std::string render_svg(const Model& model, const RenderOptions& options = {});

}  // namespace mermaid::sequence

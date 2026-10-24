// render.h - one-call facade: Mermaid text -> SVG string.
#pragma once

#include <string>
#include <string_view>

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>

namespace mermaid {

struct RenderOptions {
    std::string theme = "default";      // default | dark | forest | neutral | base
    std::string background = "white";   // CSS colour or "transparent"
    std::string svg_id = "my-svg";
    std::string css;                    // extra CSS appended to <style>
    int hand_drawn_seed = 1;            // seed of rough.js outlines (Mermaid's handDrawnSeed; 0 would be random there, here it means 1)
};

// Throws ParseError, UnsupportedDiagram or std::invalid_argument (bad theme).
std::string render_svg(std::string_view source, const RenderOptions& options = {});

namespace flowchart {

// Draws an already laid out flowchart.
std::string render_svg(const Model& model, const Layout& layout, const RenderOptions& options = {});

}  // namespace flowchart
}  // namespace mermaid

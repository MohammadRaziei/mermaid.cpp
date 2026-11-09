// pie.h - the model of a Mermaid `pie` chart (what pieDb holds after parsing).
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace mermaid::pie {

struct Section {
    std::string label;
    double value = 0;
};

// The `pie` keys of an `%%{init}%%` directive (Mermaid's PieDiagramConfig); unset ones keep Mermaid's defaults.
struct Config {
    std::optional<double> text_position;           // 0 at the centre to 1 at the rim (0.75)
    std::optional<double> donut_hole;              // 0 to 0.9 (0)
    std::optional<std::string> legend_position;    // top | bottom | left | right | center (right)
    std::optional<std::string> highlight_slice;    // a label, or `hover`
    std::optional<bool> use_max_width;             // true
    // keys that change the drawing but are not read yet: rendering throws UnsupportedDiagram for them
    std::vector<std::string> unsupported;
};

struct Model {
    std::string title;
    std::string acc_title;
    std::string acc_descr;
    bool show_data = false;
    std::vector<Section> sections;  // in source order; a repeated label keeps its first value
    Config config;
};

}  // namespace mermaid::pie

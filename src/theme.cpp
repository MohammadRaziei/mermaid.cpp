#include "theme.h"

#include <stdexcept>
#include <string>

namespace mermaid::detail {

// The default palette is Mermaid's own. The others follow the look of the
// corresponding Mermaid themes but are not exact copies.
Theme Theme::named(std::string_view name) {
    Theme t;
    t.name = std::string(name);
    if (name == "default") {
        t.node_fill = "#ECECFF";
        t.node_border = "#9370DB";
        t.text = "#333333";
        t.line = "#333333";
        t.cluster_fill = "#ffffde";
        t.cluster_border = "#aaaa33";
        t.edge_label_bg = "rgba(232,232,232,0.8)";
    } else if (name == "dark") {
        t.node_fill = "#1f2020";
        t.node_border = "#81B1DB";
        t.text = "#cccccc";
        t.line = "#f4f4f4";
        t.cluster_fill = "#474848";
        t.cluster_border = "rgba(255,255,255,0.25)";
        t.edge_label_bg = "#585858";
    } else if (name == "forest") {
        t.node_fill = "#cde498";
        t.node_border = "#13540c";
        t.text = "#000000";
        t.line = "#008000";
        t.cluster_fill = "#cdffb2";
        t.cluster_border = "#6eaa49";
        t.edge_label_bg = "#e8e8e8";
    } else if (name == "neutral") {
        t.node_fill = "#eeeeee";
        t.node_border = "#999999";
        t.text = "#333333";
        t.line = "#666666";
        t.cluster_fill = "#f4f4f4";
        t.cluster_border = "#bbbbbb";
        t.edge_label_bg = "#e8e8e8";
    } else {
        throw std::invalid_argument("unknown theme '" + std::string(name) + "' (default, dark, forest, neutral)");
    }
    return t;
}

}  // namespace mermaid::detail

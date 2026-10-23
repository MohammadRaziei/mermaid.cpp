// node_sizer.h (private) - outer box of a flowchart node from its label.
#pragma once

#include <mermaid/flowchart.h>
#include <mermaid/flowchart_layout.h>
#include <mermaid/text_measurer.h>

namespace mermaid::flowchart::detail {

// One place that knows how big each shape is. Numbers for the plain box and the
// diamond come from the merman layout goldens; the other shapes follow the same
// idea (label box + padding, plus the room their outline needs).
class NodeSizer {
public:
    NodeSizer(const TextMeasurer& measurer, const LayoutOptions& options)
        : measurer_(measurer), options_(options) {}

    struct Label {
        Size size;
        bool wrapped = false;  // the text had to break to fit the label width
    };

    Size size(const Node& node) const;
    Label label(const Node& node) const;
    Size label_size(const Node& node) const { return label(node).size; }

private:
    const TextMeasurer& measurer_;
    const LayoutOptions& options_;
};

}  // namespace mermaid::flowchart::detail

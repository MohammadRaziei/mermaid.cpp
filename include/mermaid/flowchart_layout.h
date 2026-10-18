// flowchart_layout.h - flowchart::Model -> positioned geometry.
#pragma once

#include <string>
#include <vector>

#include <mermaid/flowchart.h>
#include <mermaid/geometry.h>
#include <mermaid/text_measurer.h>

namespace mermaid::flowchart {

struct LayoutOptions {
    double node_sep = 40;         // gap between nodes of one rank
    double rank_sep = 40;         // gap between ranks
    double edge_sep = 20;         // gap used around edge dummies
    double margin = 12;           // space around the drawing
    double pad_x = 16;            // label -> box padding (each side)
    double pad_y = 12;
    double diamond_pad = 15;      // diamond: (w+pad) + (h+pad)
    // Layout goldens of merman give 152 = 120 + 2*16 for `A[Start]`: labels
    // are never narrower than this. (Source of the 120 not yet understood.)
    double min_label_width = 120;
    double cluster_pad = 8;
    // Goldens of merman reserve the label plus this much on every side in the label rank
    // (basic: 101 = 40 + 21 + 40; fixture 003: 99.74 = 40 + 19.74 + 40). Cause not yet confirmed.
    double edge_label_pad = 20;
};

struct NodeLayout {
    std::string id;
    Shape shape = Shape::Default;
    Point center;
    Size size;
    Rect rect() const { return Rect::from_center(center, size); }
};

struct EdgeLayout {
    std::string id;
    std::vector<Point> points;  // source border -> ... -> target border
    bool has_label = false;
    Rect label;                 // valid when has_label
};

struct ClusterLayout {
    std::string id;
    std::string title;
    Rect rect;
};

struct Layout {
    std::vector<NodeLayout> nodes;      // model order, subgraph ids excluded
    std::vector<EdgeLayout> edges;      // model order
    std::vector<ClusterLayout> clusters;  // outermost last
    Rect bounds;                        // union of nodes, labels and clusters
    Size size;                          // bounds plus margin on the far sides

    const NodeLayout* find_node(const std::string& id) const;
};

class LayoutEngine {
public:
    explicit LayoutEngine(const TextMeasurer& measurer, LayoutOptions options = {})
        : measurer_(measurer), options_(options) {}

    Layout layout(const Model& model) const;

private:
    const TextMeasurer& measurer_;
    LayoutOptions options_;
};

}  // namespace mermaid::flowchart

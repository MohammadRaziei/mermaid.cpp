// flowchart_layout.h - flowchart::Model -> positioned geometry.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include <mermaid/flowchart.h>
#include <mermaid/geometry.h>
#include <mermaid/text_measurer.h>

namespace mermaid::flowchart {

struct LayoutOptions {
    double node_sep = 50;         // gap between nodes of one rank (flowchart.nodeSpacing)
    double rank_sep = 50;         // gap between ranks (flowchart.rankSpacing)
    double edge_sep = 20;         // gap used around edge dummies
    double margin = 8;            // space around the drawing (dagre marginx/marginy)
    double pad_x = 16;            // label -> box padding (each side)
    double pad_y = 12;
    double diamond_pad = 15;      // diamond: (w+pad) + (h+pad)
    double edge_label_max_width = 200;  // edge label wrap width (max-width of the label div)
    double node_padding = 15;     // flowchart.padding: round, lean, trapezoid shapes
    // Layout goldens of merman give 152 = 120 + 2*16 for `A[Start]`: labels
    // are never narrower than this. (Source of the 120 not yet understood.)
    double min_label_width = 120;
    double cluster_pad = 8;
    // Extra room around an edge label in the label rank. Real Mermaid (dagre) reserves none; the
    // merman/ELK goldens needed 20.
    double edge_label_pad = 0;
};

struct NodeLayout {
    std::string id;
    Shape shape = Shape::Default;
    Point center;
    Size size;
    Size label;  // the label box inside the outline (text, at least `min_label_width` wide)
    bool label_wrapped = false;  // the text broke into lines to fit the label width
    Rect rect() const { return Rect::from_center(center, size); }
};

struct EdgeLayout {
    std::string id;
    size_t model_index = 0;     // index into Model::edges
    bool self_loop = false;     // a loop on one node: `points` is the synthesised 4-point route (see Layout::loop_helpers)
    std::vector<Point> points;  // source border -> ... -> target border
    bool has_label = false;
    bool label_wrapped = false;  // the label text broke into lines at `edge_label_max_width`
    Rect label;                 // valid when has_label
};

struct ClusterLayout {
    std::string id;
    std::string title;
    bool markdown_title = false;
    Rect rect;            // the dagre compound box
    Size label;           // measured title, zero without a title
    int color_index = 0;  // declaration order, depth first (Mermaid's `colorIndex`)
};

// The two tiny nodes Mermaid lays a self-loop out with (0.1 x 0.1 once measured; they take part in the layout and are
// drawn, empty).
struct LoopHelper {
    std::string id;  // "<node>---<node>---1" / "---2"
    Point center;
};

struct Layout;

// A subgraph with no edge leaving it is laid out as a graph of its own (Mermaid's recursive render, drawn as a
// nested `g.root`) and is then a single node of the parent graph.
struct RootLayout {
    std::string id;                  // the subgraph
    Rect box;                        // its box in the parent's coordinates
    Point origin;                    // translate that moves the inner coordinates onto `box`
    std::shared_ptr<const Layout> layout;  // nodes, edges and clusters in the inner coordinates
};

struct Layout {
    std::vector<NodeLayout> nodes;      // model order, subgraph ids excluded; nodes inside `roots` are not here
    std::vector<EdgeLayout> edges;      // the edges of this graph, model order
    std::vector<ClusterLayout> clusters;  // outermost last
    std::vector<LoopHelper> loop_helpers;  // two per self-loop, in dagre node order
    std::vector<RootLayout> roots;      // closed subgraphs, in dagre node order (reverse declaration order)
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

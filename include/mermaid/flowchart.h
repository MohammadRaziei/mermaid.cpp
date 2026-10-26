// flowchart.h - semantic model of a flowchart (parser output, layout input).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace mermaid::flowchart {

enum class Direction { TB, BT, LR, RL };

// Names follow merman's semantic goldens ("square" for `[...]`, ...).
enum class Shape {
    Default,  // plain `A` (drawn as a rectangle)
    Square,
    Round,
    Stadium,
    Subroutine,
    Cylinder,
    Circle,
    DoubleCircle,
    Diamond,
    Hexagon,
    Odd,
    LeanRight,
    LeanLeft,
    Trapezoid,
    InvTrapezoid,
    Hourglass,         // `@{ shape: hourglass }` (collate)
    Triangle,          // `tri` (extract)
    FlippedTriangle,   // `flip-tri` (manual file)
    Text,              // `text`: a label with no outline
    Fork,              // `fork`, `join`: a thin bar across the flow
    SmallCircle,       // `sm-circ` (start): a dot with a label-less 14px circle
    Document,          // `doc` (waveEdgedRectangle): a box whose bottom edge is a wave
    Documents,         // `docs`, `st-doc` (multiWaveEdgedRectangle): two stacked sheets
    TaggedDocument,    // `tag-doc` (taggedWaveEdgedRectangle): a document with a tag in the corner
    LinedDocument,     // `lin-doc` (linedWaveEdgedRect): a document with a margin line
    Flag,              // `flag`, `paper-tape` (waveRectangle): both long edges are waves
    Delay,             // `delay` (halfRoundedRectangle): a box with a round right end
    NotchedPentagon,   // `notch-pent` (trapezoidalPentagon): cut top corners
    SlopedRectangle,   // `sl-rect` (slopedRect): a rectangle whose top edge slopes
    NotchedRectangle,  // `notch-rect` (card): a rectangle with a cut top left corner
    LinedRectangle,    // `lin-rect` (shadedProcess): a rectangle with a bar on the left
    DividedRectangle,  // `div-rect` (dividedRectangle): a rectangle with a divider near the top
    TiltedCylinder,    // `h-cyl` (tiltedCylinder): a cylinder lying on its side
    LinedCylinder,     // `lin-cyl` (linedCylinder): a cylinder with a second lid line
    StackedRectangle,  // `st-rect` (multiRect): two stacked boxes
    TaggedRectangle,   // `tag-rect` (taggedRect): a box with a tag in the corner
    BowTieRectangle,   // `bow-rect` (bowTieRect): a box with both short sides curved inwards
    LightningBolt,     // `bolt` (lightningBolt): a label-less 35 x 70 zigzag
    Cloud,             // `cloud`: a path of circular arcs (a mindmap shape)
    Bang,              // `bang`: a spiky path of circular arcs (a mindmap shape)
    BraceLeft,         // `brace`, `brace-l`, `comment` (curlyBraceLeft): a curly brace, no fill
    BraceRight,        // `brace-r` (curlyBraceRight)
    Braces,            // `braces` (curlyBraces): both braces
    WindowPane,        // `win-pane` (windowPane): a box with a cross of lines at its top left
    FilledCircle,      // `f-circ` (filledCircle): a label-less filled dot of 14 px
    FramedCircle,      // `fr-circ` (stateEnd): a label-less ring with a dot inside
    CrossedCircle,     // `cross-circ` (crossedCircle): a label-less circle of 60 px with a cross
    CurvedTrapezoid,   // `curv-trap` (curvedTrapezoid): a pointed left end and a round right end
};

enum class Stroke { Normal, Thick, Dotted, Invisible };
enum class Head { None, Arrow, Cross, Circle };

// `fill:#f9f` -> {"fill", "#f9f"}, in source order.
using Style = std::vector<std::pair<std::string, std::string>>;

struct Node {
    std::string id;
    std::string label;
    Shape shape = Shape::Default;
    std::vector<std::string> classes;
    Style style;  // from `style` statements
    bool markdown_label = false;  // label set by `@{ label: ... }` (labelType defaults to markdown)
    int dom_index = 0;  // n of the DOM id `flowchart-<id>-<n>` (Mermaid's vertex counter at first sight)
};

struct Edge {
    std::string id;  // "L_A_B_0" or the user supplied `e1@`
    std::string from;
    std::string to;
    std::string label;
    Stroke stroke = Stroke::Normal;
    Head start = Head::None;
    Head end = Head::Arrow;
    int length = 1;  // extra dashes make an edge span more ranks
    Style style;     // from `linkStyle`
    std::string curve;  // from `e1@{curve: ...}`; empty = the default (basis)
    bool markdown_label = false;  // the label was a "`...`" string
};

struct Subgraph {
    std::string id;
    std::string title;
    bool markdown_title = false;  // the title was a "`...`" string
    std::vector<std::string> members;  // node ids and nested subgraph ids
    std::vector<std::string> classes;  // from `class` statements naming the subgraph id
    std::optional<Direction> direction;
};

struct ClassDef {
    std::string name;
    Style style;
};

// The `%%{init: ...}%%` settings the flowchart renderer reads (later directives override earlier ones).
struct InitConfig {
    std::optional<std::string> theme;           // theme: default, dark, forest, neutral, base
    std::optional<std::string> font_family;     // themeVariables.fontFamily
    std::optional<std::string> cluster_bkg;     // themeVariables.clusterBkg
    std::optional<std::string> cluster_border;  // themeVariables.clusterBorder
    std::optional<double> node_spacing;         // flowchart.nodeSpacing
    std::optional<double> rank_spacing;         // flowchart.rankSpacing
    std::optional<std::string> curve;           // flowchart.curve: default curve of edges without their own
};

struct Model {
    Direction direction = Direction::TB;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Subgraph> subgraphs;
    std::map<std::string, ClassDef> class_defs;
    std::vector<std::string> class_def_order;  // names in first-definition order
    std::string acc_title;
    std::string acc_descr;
    InitConfig config;

    const Node* find_node(const std::string& id) const;
    const Subgraph* find_subgraph(const std::string& id) const;
};

const char* shape_name(Shape shape) noexcept;       // "" for Shape::Default
const char* direction_name(Direction d) noexcept;   // "TB", "BT", "LR", "RL"
// merman edge type: arrow_point, arrow_open, arrow_cross, arrow_circle,
// double_arrow_point, double_arrow_cross, double_arrow_circle.
std::string edge_type_name(const Edge& edge);
const char* stroke_name(Stroke s) noexcept;         // normal, thick, dotted, invisible

// "fill:#f9f, stroke:#333;" -> declarations. Commas inside parentheses
// (rgb(1,2,3)) do not split.
Style parse_style(const std::string& text);

}  // namespace mermaid::flowchart

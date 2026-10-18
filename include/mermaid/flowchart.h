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
};

struct Subgraph {
    std::string id;
    std::string title;
    std::vector<std::string> members;  // node ids and nested subgraph ids
    std::optional<Direction> direction;
};

struct ClassDef {
    std::string name;
    Style style;
};

struct Model {
    Direction direction = Direction::TB;
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Subgraph> subgraphs;
    std::map<std::string, ClassDef> class_defs;
    std::string acc_title;
    std::string acc_descr;

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

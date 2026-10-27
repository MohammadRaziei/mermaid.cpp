#include <mermaid/flowchart.h>

#include "preprocess.h"

namespace mermaid::flowchart {

const Node* Model::find_node(const std::string& id) const {
    for (const Node& n : nodes)
        if (n.id == id) return &n;
    return nullptr;
}

const Subgraph* Model::find_subgraph(const std::string& id) const {
    for (const Subgraph& s : subgraphs)
        if (s.id == id) return &s;
    return nullptr;
}

const char* shape_name(Shape shape) noexcept {
    switch (shape) {
        case Shape::Default: return "";
        case Shape::Square: return "square";
        case Shape::Round: return "round";
        case Shape::Stadium: return "stadium";
        case Shape::Subroutine: return "subroutine";
        case Shape::Cylinder: return "cylinder";
        case Shape::Circle: return "circle";
        case Shape::DoubleCircle: return "doublecircle";
        case Shape::Diamond: return "diamond";
        case Shape::Hexagon: return "hexagon";
        case Shape::Odd: return "odd";
        case Shape::LeanRight: return "lean_right";
        case Shape::LeanLeft: return "lean_left";
        case Shape::Trapezoid: return "trapezoid";
        case Shape::InvTrapezoid: return "inv_trapezoid";
        case Shape::Hourglass: return "hourglass";
        case Shape::Triangle: return "triangle";
        case Shape::FlippedTriangle: return "flippedTriangle";
        case Shape::Text: return "text";
        case Shape::Fork: return "forkJoin";
        case Shape::SmallCircle: return "stateStart";
        case Shape::Document: return "waveEdgedRectangle";
        case Shape::Documents: return "multiWaveEdgedRectangle";
        case Shape::TaggedDocument: return "taggedWaveEdgedRectangle";
        case Shape::LinedDocument: return "linedWaveEdgedRect";
        case Shape::Flag: return "waveRectangle";
        case Shape::FilledCircle: return "filledCircle";
        case Shape::FramedCircle: return "stateEnd";
        case Shape::CrossedCircle: return "crossedCircle";
        case Shape::WindowPane: return "windowPane";
        case Shape::BraceLeft: return "curlyBraceLeft";
        case Shape::BraceRight: return "curlyBraceRight";
        case Shape::Braces: return "curlyBraces";
        case Shape::Cloud: return "cloud";
        case Shape::Bang: return "bang";
        case Shape::LightningBolt: return "lightningBolt";
        case Shape::BowTieRectangle: return "bowTieRect";
        case Shape::StackedRectangle: return "multiRect";
        case Shape::TaggedRectangle: return "taggedRect";
        case Shape::TiltedCylinder: return "tiltedCylinder";
        case Shape::LinedCylinder: return "linedCylinder";
        case Shape::CurvedTrapezoid: return "curvedTrapezoid";
        case Shape::Folder: return "folder";
        case Shape::Bucket: return "bucket";
        case Shape::NotchedRectangle: return "card";
        case Shape::LinedRectangle: return "shadedProcess";
        case Shape::DividedRectangle: return "dividedRectangle";
        case Shape::Delay: return "halfRoundedRectangle";
        case Shape::NotchedPentagon: return "trapezoidalPentagon";
        case Shape::SlopedRectangle: return "slopedRect";
    }
    return "";
}

const char* direction_name(Direction d) noexcept {
    switch (d) {
        case Direction::TB: return "TB";
        case Direction::BT: return "BT";
        case Direction::LR: return "LR";
        case Direction::RL: return "RL";
    }
    return "TB";
}

const char* stroke_name(Stroke s) noexcept {
    switch (s) {
        case Stroke::Normal: return "normal";
        case Stroke::Thick: return "thick";
        case Stroke::Dotted: return "dotted";
        case Stroke::Invisible: return "invisible";
    }
    return "normal";
}

std::string edge_type_name(const Edge& edge) {
    const char* base = "arrow_point";
    switch (edge.end) {
        case Head::None: base = "arrow_open"; break;
        case Head::Arrow: base = "arrow_point"; break;
        case Head::Cross: base = "arrow_cross"; break;
        case Head::Circle: base = "arrow_circle"; break;
    }
    if (edge.start == Head::None) return base;
    return std::string("double_") + base;
}

Style parse_style(const std::string& text) {
    Style out;
    std::string piece;
    int depth = 0;
    auto flush = [&]() {
        std::string t = detail::trim(piece);
        piece.clear();
        while (!t.empty() && t.back() == ';') t.pop_back();
        size_t colon = t.find(':');
        if (colon == std::string::npos) return;
        std::string key = detail::trim(t.substr(0, colon));
        std::string value = detail::trim(t.substr(colon + 1));
        if (key.empty() || value.empty()) return;
        out.emplace_back(key, value);
    };
    for (char c : text) {
        if (c == '(') ++depth;
        else if (c == ')' && depth > 0) --depth;
        if (c == ',' && depth == 0) flush();
        else piece += c;
    }
    flush();
    return out;
}

}  // namespace mermaid::flowchart

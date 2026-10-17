#include "node_sizer.h"

#include <algorithm>

namespace mermaid::flowchart::detail {

Size NodeSizer::size(const Node& node) const {
    const Size label = measurer_.measure(node.label);
    const double text_w = std::max(label.width, options_.min_label_width);
    const double text_h = label.height;

    const double box_w = text_w + 2 * options_.pad_x;
    const double box_h = text_h + 2 * options_.pad_y;

    switch (node.shape) {
        case Shape::Diamond: {
            const double side = (text_w + options_.diamond_pad) + (text_h + options_.diamond_pad);
            return {side, side};
        }
        case Shape::Circle:
            return {box_w, box_w};
        case Shape::DoubleCircle:
            return {box_w + 10, box_w + 10};
        case Shape::Hexagon:
        case Shape::Odd:
        case Shape::LeanRight:
        case Shape::LeanLeft:
        case Shape::Trapezoid:
        case Shape::InvTrapezoid:
            return {box_w + box_h / 2, box_h};
        case Shape::Cylinder:
            return {box_w, box_h + 16};
        case Shape::Default:
        case Shape::Square:
        case Shape::Round:
        case Shape::Stadium:
        case Shape::Subroutine:
            break;
    }
    return {box_w, box_h};
}

}  // namespace mermaid::flowchart::detail

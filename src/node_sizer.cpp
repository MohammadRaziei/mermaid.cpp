#include "node_sizer.h"

#include "markdown_label.h"

#include <algorithm>
#include <cmath>
#include <mermaid/macros.h>

#include "shape_geometry.h"

namespace mermaid::flowchart::detail {

namespace {
constexpr double M_PI_ = 3.14159265358979323846;
}

// labelHelper + createText: the text wraps at the label width (120); a label that fits is as wide as its text, and
// all shapes but the circles hold it to at least that width.
NodeSizer::Label NodeSizer::label(const Node& node) const {
    if (node.shape == Shape::Hourglass || node.shape == Shape::Fork || node.shape == Shape::SmallCircle)
        return {{0, 0}, false};  // these shapes have no label (hourglass.ts empties it; the others never make one)
    const TextMeasurer::Wrapped text = node.markdown_label
                                           ? mermaid::detail::markdown_label(node.label).measure(measurer_, options_.min_label_width)
                                           : measurer_.measure_wrapped(node.label, options_.min_label_width);
    if (mermaid::detail::ignores_min_label_width(node.shape)) return {text.size, text.wrapped};
    return {{MERMAID_MAX(text.size.width, options_.min_label_width), text.size.height}, text.wrapped};
}

// The sizes follow shapes/*.ts of Mermaid 12.2 in the neo look.
Size NodeSizer::size(const Node& node) const {
    const Size label = label_size(node);
    const double text_w = label.width;
    const double text_h = label.height;
    const double p = options_.node_padding;

    switch (node.shape) {
        case Shape::Diamond: {
            const double side = (text_w + options_.diamond_pad) + (text_h + options_.diamond_pad);
            return {side, side};
        }
        case Shape::Circle: {  // circle.ts: the radius is half the diagonal of the label, plus 16
            const double r = std::hypot(text_w, text_h) / 2 + 16;
            return {2 * r, 2 * r};
        }
        case Shape::DoubleCircle: {  // doubleCircle.ts: the inner ring like the circle, the outer one 12 further
            const double r = std::hypot(text_w, text_h) / 2 + 16 + 12;
            return {2 * r, 2 * r};
        }
        case Shape::Round:  // roundedRect.ts: drawRect with the node padding on every side
            return {text_w + 2 * p, text_h + 2 * p};
        case Shape::Subroutine:  // subroutine.ts: 8 frame bars on each side, labelPadding 28 x 12
            return {text_w + 2 * 8 + 28, text_h + 12};
        case Shape::Hexagon: {  // hexagon.ts (neo): h = text + 70, m = h / 3.5, w = text + 2m + 32
            const double h = text_h + 70;
            return {text_w + 2 * (h / 3.5) + 32, h};
        }
        case Shape::Odd: {  // rectLeftInvArrow.ts (neo): label padding 21 x 12, the notch adds h / 4
            const double h = text_h + 24;
            return {text_w + 42 + h / 4, h};
        }
        case Shape::LeanRight:  // lean_right.ts, lean_left.ts, trapezoid.ts: h = text + p, w = text + 2p, then + h
        case Shape::LeanLeft:
        case Shape::Trapezoid: {
            const double h = text_h + p;
            return {text_w + 2 * p + h, h};
        }
        case Shape::InvTrapezoid: {  // invertedTrapezoid.ts: padding on both sides
            const double h = text_h + 2 * p;
            return {text_w + 4 * p + h, h};
        }
        case Shape::Stadium: {
            // shapes/stadium.ts (neo): paddingX 40, paddingY 24; the arcs are sampled with 50 points,
            // so their inscribed circle is a little smaller than the circle.
            const double h = text_h + 24;
            const double inscribed = h * std::cos(M_PI_ / (2 * 49));
            const double cap = std::sqrt(MERMAID_MAX(0.0, inscribed * inscribed - text_h * text_h));
            const double w = MERMAID_MAX(MERMAID_MAX(text_w + h / 4 + 40, 1.5 * h + (h - inscribed)), text_w + 40 + h - cap);
            return {w, h};
        }
        case Shape::Cylinder: {
            const double pad = 2 * options_.pad_y;  // 24 in the neo look, like the box height padding
            const double w = text_w + pad;
            const double ry = (w / 2) / (2.5 + w / 50);  // the two ellipses add 3 * ry to the height
            return {w, text_h + pad + 3 * ry};
        }
        case Shape::Hourglass:  // hourglass.ts: 30 x 30
            return {30, 30};
        case Shape::Fork:  // forkJoin.ts: 70 x 10, turned in LR and RL
            return direction_ == Direction::LR || direction_ == Direction::RL ? Size{10, 70} : Size{70, 10};
        case Shape::SmallCircle:  // stateStart.ts: 14 x 14
            return {14, 14};
        case Shape::Text:  // text.ts: the label plus the padding once
            return {text_w + p, text_h + p};
        case Shape::Triangle:  // triangle.ts (neo): w = text + 2 * padding, h = w + text height; the outline is h wide
        case Shape::FlippedTriangle: {
            const double h = text_w + 2 * p + text_h;
            return {h, h};
        }
        case Shape::Default:
        case Shape::Square:
            break;
    }
    return {text_w + 2 * options_.pad_x, text_h + 2 * options_.pad_y};
}

}  // namespace mermaid::flowchart::detail

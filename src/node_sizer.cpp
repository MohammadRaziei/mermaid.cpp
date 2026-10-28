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
    if (node.shape == Shape::Hourglass || node.shape == Shape::Fork || node.shape == Shape::SmallCircle ||
        node.shape == Shape::LightningBolt || node.shape == Shape::FilledCircle || node.shape == Shape::FramedCircle ||
        node.shape == Shape::CrossedCircle)
        return {{0, 0}, false};  // these shapes have no label (hourglass.ts empties it; the others never make one)
    const TextMeasurer::Wrapped text = node.markdown_label
                                           ? mermaid::detail::markdown_label(node.label).measure(measurer_, options_.min_label_width)
                                           : measurer_.measure_wrapped(node.label, options_.min_label_width);
    if (mermaid::detail::ignores_min_label_width(node.shape)) return {text.size, text.wrapped};
    return {{MERMAID_MAX(text.size.width, options_.min_label_width), text.size.height}, text.wrapped};
}

// The sizes follow shapes/*.ts of Mermaid 12.2 in the neo look.
Size NodeSizer::size(const Node& node, Direction direction) const {
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
            return direction == Direction::LR || direction == Direction::RL ? Size{10, 70} : Size{70, 10};
        case Shape::SmallCircle:  // stateStart.ts: 14 x 14
        case Shape::FilledCircle:  // filledCircle.ts: radius 7; stateEnd.ts: width 14
        case Shape::FramedCircle:
            return {14, 14};
        case Shape::CrossedCircle:  // crossedCircle.ts: radius max(30, width)
            return {60, 60};
        case Shape::Text:  // text.ts: the label plus the padding once
            return {text_w + p, text_h + p};
        case Shape::Triangle:  // triangle.ts (neo): w = text + 2 * padding, h = w + text height; the outline is h wide
        case Shape::FlippedTriangle: {
            const double h = text_w + 2 * p + text_h;
            return {h, h};
        }
        case Shape::Document: {  // waveEdgedRectangle.ts (neo): padding 16 x 12, the wave adds a quarter of the box height twice
            const double h = text_h + 2 * 12;
            return {text_w + 2 * 16, h + h / 2};
        }
        case Shape::Documents: {  // multiWaveEdgedRectangle.ts (neo): padding 16 x 12 x 3, two sheets 10 px apart
            const double h = text_h + 3 * 12;
            return {text_w + 2 * 16 + 20, h + 1.5 * (h / 4) + 20};
        }
        case Shape::TaggedDocument: {  // taggedWaveEdgedRectangle.ts: node padding on every side, wave h / 8, 10 % margin
            const double w = text_w + 2 * p, h = text_h + 2 * p;
            return {1.1 * w, h + 2 * (h / 8)};
        }
        case Shape::LinedDocument: {  // linedWaveEdgedRect.ts (neo): padding 16 x 12, wave h / 4, 10 % margin
            const double w = text_w + 2 * 16, h = text_h + 2 * 12;
            return {1.1 * w, h + 2 * (h / 4)};
        }
        case Shape::Flag: {  // waveRectangle.ts (neo): padding 16 x 20, two waves of h / 8 each
            const double h = text_h + 20;
            return {text_w + 2 * 16, h + 4 * (h / 8)};
        }
        case Shape::Delay: {  // halfRoundedRectangle.ts (neo): the round end clears the label by capClearance on both sides
            const double h = MERMAID_MAX(10.0, text_h) + 2 * 12;
            const double r = h / 2;
            const double clearance = r - std::sqrt(MERMAID_MAX(0.0, r * r - (text_h / 2) * (text_h / 2)));
            return {MERMAID_MAX(15.0, text_w) + clearance * 2 + 2 * 16, h};
        }
        case Shape::NotchedPentagon:  // trapezoidalPentagon.ts (neo): padding 16 x 12
            return {text_w + 2 * 16, text_h + 2 * 12};
        case Shape::SlopedRectangle:  // slopedRect.ts (neo): the box is 1.5 times its height
            return {text_w + 2 * 16, (text_h + 2 * 12) * 1.5};
        case Shape::NotchedRectangle:  // card.ts (neo): padding 28 x 24
            return {text_w + 2 * 28, text_h + 2 * 24};
        case Shape::LinedRectangle:  // shadedProcess.ts (neo): padding 16 x 12 plus the 8 px bar
            return {text_w + 2 * 16 + 8, text_h + 2 * 12};
        case Shape::DividedRectangle: {  // dividedRectangle.ts (neo): padding 16, the divider adds 20 % of the height
            const double h = text_h + 16;
            return {text_w + 16, h + h * 0.2};
        }
        case Shape::CurvedTrapezoid: {  // curvedTrapezoid.ts (neo): padding 16 x 12, the round end clears the label
            const double h = MERMAID_MAX(5.0, text_h + 2 * 12), r = h / 2;
            const double clearance = r - std::sqrt(MERMAID_MAX(0.0, r * r - (text_h / 2) * (text_h / 2)));
            const double side = MERMAID_MAX(h / 4, clearance);
            const double w = MERMAID_MAX(MERMAID_MAX(20.0, text_w + 2 * 16 + side * 2), (text_w + 2 * 16) * 1.25);
            return {w, h};
        }
        case Shape::DataStore:  // datastore.ts: drawRect with labelPaddingX = 2 * padding, so 4 * padding wide
            return {text_w + 4 * p, text_h + 2 * p};
        case Shape::Person: {
            const auto g = mermaid::detail::person_geometry_of({text_w, text_h});
            return {g.w, g.total};
        }
        case Shape::Console:  // consoleWindow.ts: a 20 band for the glyph above the body
        case Shape::Browser:  // browser.ts: an 18 title bar above the body
            return {MERMAID_MAX(text_w + 2 * p, 90.0), text_h + 2 * p + (node.shape == Shape::Console ? 20.0 : 18.0)};
        case Shape::Bucket: {  // bucket.ts: the rim ellipse sticks out above and the bottom arc below, so the box is rimRy higher
            const double w = MERMAID_MAX(text_w + 2 * p, 80.0), rim = MERMAID_MAX(MERMAID_MIN(w * 0.08, 12.0), 5.0);
            return {w, text_h + 2 * p + 2 * rim};
        }
        case Shape::Folder: {  // folder.ts: padding 15, a tab of 16 % of the content height (8 to 14) on top, at least 90 wide
            const double content = text_h + 2 * p;
            return {MERMAID_MAX(text_w + 2 * p, 90.0), content + MERMAID_MAX(MERMAID_MIN(content * 0.16, 14.0), 8.0)};
        }
        case Shape::TiltedCylinder: {  // tiltedCylinder.ts (neo): label padding 12, the two lids add 2 * rx on each side
            const double h = text_h + 12, ry = h / 2, rx = ry / (2.5 + h / 50);
            return {text_w + rx + 12 + 2 * rx, h};
        }
        case Shape::LinedCylinder: {  // linedCylinder.ts (neo): padding 16 x 24, the two lids add 2 * ry
            const double w = text_w + 2 * 16, rx = w / 2, ry = rx / (2.5 + w / 50);
            return {w, text_h + ry + 2 * 24 + 2 * ry};
        }
        case Shape::StackedRectangle:  // multiRect.ts (neo): padding 16 x 12, the second box sticks out 10 on each side
            return {text_w + 2 * 16 + 20, text_h + 2 * 12 + 20};
        case Shape::TaggedRectangle: {  // taggedRect.ts (neo): the tag is a fifth of the height wide
            const double h = text_h + 2 * 12;
            return {text_w + 2 * 16 + 0.2 * h, h};
        }
        case Shape::BowTieRectangle: {  // bowTieRect.ts (neo): the left arc bulges out of the box; getBBox sees the sampled points
            const auto outline = mermaid::detail::polygon_outline_of(Shape::BowTieRectangle, {0, 0}, label);
            double min_x = outline->points.front().x, max_x = min_x;
            for (const Point& p : outline->points) {
                min_x = MERMAID_MIN(min_x, p.x);
                max_x = MERMAID_MAX(max_x, p.x);
            }
            return {max_x - min_x, text_h + 12};
        }
        case Shape::LightningBolt:  // lightningBolt.ts: no label, 35 wide, the zigzag is 2 * 35 high
            return {35, 70};
        case Shape::BraceLeft:  // curlyBrace*.ts: the node is the box of the invisible outline round the braces
        case Shape::BraceRight:
        case Shape::Braces: {
            const auto outline = mermaid::detail::polygon_outline_of(node.shape, {0, 0}, label);
            double min_x = outline->points.front().x, max_x = min_x, min_y = outline->points.front().y, max_y = min_y;
            for (const auto& layer : outline->layers) {  // the braces can stick out of the invisible box
                for (const Point& p : layer.points) {
                    min_x = MERMAID_MIN(min_x, p.x); max_x = MERMAID_MAX(max_x, p.x);
                    min_y = MERMAID_MIN(min_y, p.y); max_y = MERMAID_MAX(max_y, p.y);
                }
            }
            return {max_x - min_x, max_y - min_y};
        }
        case Shape::WindowPane:  // windowPane.ts (neo): padding 16 x 12 plus rectOffset 10 on the top left
            return {text_w + 2 * 16 + 10, text_h + 2 * 12 + 10};
        case Shape::Cloud:  // cloud.ts / bang.ts: the node is the bounding box of the arcs
        case Shape::Bang:
            return mermaid::detail::arc_shape_of(node.shape, label)->bounds;
        case Shape::Default:
        case Shape::Square:
            break;
    }
    return {text_w + 2 * options_.pad_x, text_h + 2 * options_.pad_y};
}

}  // namespace mermaid::flowchart::detail

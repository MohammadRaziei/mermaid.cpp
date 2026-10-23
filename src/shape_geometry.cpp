#include "shape_geometry.h"

#include <algorithm>
#include <cmath>

namespace mermaid::detail {

using flowchart::Shape;

namespace {

constexpr double kSubroutineFrame = 8;  // subroutine.ts FRAME_WIDTH

// intersect-line.js
std::optional<Point> intersect_line(const Point& p1, const Point& p2, const Point& q1, const Point& q2) {
    const double a1 = p2.y - p1.y, b1 = p1.x - p2.x, c1 = p2.x * p1.y - p1.x * p2.y;
    const double r3 = a1 * q1.x + b1 * q1.y + c1, r4 = a1 * q2.x + b1 * q2.y + c1;
    constexpr double epsilon = 1e-6;
    if (r3 != 0 && r4 != 0 && r3 * r4 > 0) return std::nullopt;

    const double a2 = q2.y - q1.y, b2 = q1.x - q2.x, c2 = q2.x * q1.y - q1.x * q2.y;
    const double r1 = a2 * p1.x + b2 * p1.y + c2, r2 = a2 * p2.x + b2 * p2.y + c2;
    if (std::fabs(r1) < epsilon && std::fabs(r2) < epsilon && r1 * r2 > 0) return std::nullopt;

    const double denom = a1 * b2 - a2 * b1;
    if (denom == 0) return std::nullopt;  // collinear
    return Point{(b1 * c2 - b2 * c1) / denom, (a2 * c1 - a1 * c2) / denom};
}

}  // namespace

bool ignores_min_label_width(Shape shape) { return shape == Shape::Circle || shape == Shape::DoubleCircle; }

std::optional<polygon_outline> polygon_outline_of(Shape shape, const Size& size) {
    polygon_outline out;
    switch (shape) {
        case Shape::Hexagon: {  // hexagon.ts: m = h / 3.5 in the neo look
            const double w = size.width, h = size.height, m = h / 3.5;
            out.points = {{m, 0}, {w - m, 0}, {w, -h / 2}, {w - m, -h}, {m, -h}, {0, -h / 2}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::LeanRight: {  // lean_right.ts
            const double h = size.height, w = size.width - h;
            out.points = {{-3 * h / 6, 0}, {w, 0}, {w + 3 * h / 6, -h}, {0, -h}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::LeanLeft: {  // lean_left.ts
            const double h = size.height, w = size.width - h;
            out.points = {{0, 0}, {w + 3 * h / 6, 0}, {w, -h}, {-3 * h / 6, -h}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::Trapezoid: {  // trapezoid.ts
            const double h = size.height, w = size.width - h;
            out.points = {{-3 * h / 6, 0}, {w + 3 * h / 6, 0}, {w, -h}, {0, -h}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::InvTrapezoid: {  // invertedTrapezoid.ts
            const double h = size.height, w = size.width - h;
            out.points = {{0, 0}, {w, 0}, {w + 3 * h / 6, -h}, {-3 * h / 6, -h}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::Subroutine: {  // subroutine.ts: the two vertical bars sit 8 outside the box
            const double w = size.width - 2 * kSubroutineFrame, h = size.height, f = kSubroutineFrame;
            out.points = {{0, 0}, {w, 0}, {w, -h}, {0, -h}, {0, 0}, {-f, 0}, {w + f, 0}, {w + f, -h}, {-f, -h}, {-f, 0}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::Odd: {  // rectLeftInvArrow.ts: a rectangle with a notch on the left
            const double h = size.height, w = size.width - h / 4;
            const double x = -w / 2, y = -h / 2, notch = y / 2;
            out.points = {{x + notch, y}, {x, 0}, {x + notch, -y}, {-x, -y}, {-x, y}};
            out.rough = true;
            out.shift = {-notch / 2, 0};
            return out;
        }
        default:
            return std::nullopt;
    }
}

Point label_offset(Shape shape, const Size& size) {
    if (shape == Shape::Odd) return {size.height / 8, 0};  // -notch / 2, notch = -h / 4
    return {0, 0};
}

std::optional<Point> polygon_border_point(const flowchart::NodeLayout& node, const Point& outside) {
    const std::optional<polygon_outline> outline = polygon_outline_of(node.shape, node.size);
    if (!outline) return std::nullopt;

    double min_x = outline->points.front().x, min_y = outline->points.front().y;
    for (const Point& p : outline->points) {
        min_x = std::min(min_x, p.x);
        min_y = std::min(min_y, p.y);
    }
    // the polygon is moved so that its bounding box is the node's box
    const double left = node.center.x - node.size.width / 2 - min_x;
    const double top = node.center.y - node.size.height / 2 - min_y;

    std::vector<Point> hits;
    const std::vector<Point>& pts = outline->points;
    for (size_t i = 0; i < pts.size(); ++i) {
        const Point& a = pts[i];
        const Point& b = pts[i + 1 < pts.size() ? i + 1 : 0];
        if (const auto hit = intersect_line(node.center, outside, {left + a.x, top + a.y}, {left + b.x, top + b.y}))
            hits.push_back(*hit);
    }
    if (hits.empty()) return node.center;
    // the intersection closest to `outside` (a stable sort keeps the first of equal distances, like JS)
    std::stable_sort(hits.begin(), hits.end(), [&](const Point& p, const Point& q) {
        return std::hypot(p.x - outside.x, p.y - outside.y) < std::hypot(q.x - outside.x, q.y - outside.y);
    });
    return hits.front();
}

}  // namespace mermaid::detail

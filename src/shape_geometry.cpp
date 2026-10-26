#include "shape_geometry.h"

#include <algorithm>
#include <cmath>

namespace mermaid::detail {

using flowchart::Shape;

namespace {

constexpr double kSubroutineFrame = 8;  // subroutine.ts FRAME_WIDTH
constexpr double kNodePadding = 15;     // the flowchart padding handed to every shape as node.padding

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

// generateFullSineWavePoints (50 steps)
std::vector<Point> sine_wave_points(double x1, double y1, double x2, double y2, double amplitude, double cycles) {
    constexpr double pi = 3.14159265358979323846;
    std::vector<Point> points;
    const double delta_x = x2 - x1;
    const double frequency = 2 * pi / (delta_x / cycles);
    const double mid_y = y1 + (y2 - y1) / 2;
    for (int i = 0; i <= 50; ++i) {
        const double x = x1 + (i / 50.0) * delta_x;
        points.push_back({x, mid_y + amplitude * std::sin(frequency * (x - x1))});
    }
    return points;
}

}  // namespace

bool ignores_min_label_width(Shape shape) { return shape == Shape::Circle || shape == Shape::DoubleCircle; }

bool writes_tight_label_transform(Shape shape) {
    return shape == Shape::Document || shape == Shape::TaggedDocument || shape == Shape::LinedDocument;  // multiWave writes `, `
}

std::optional<polygon_outline> polygon_outline_of(Shape shape, const Size& size, const Size& label) {
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
        case Shape::Hourglass: {  // hourglass.ts: the corners of a 30 x 30 box in the order of an hourglass
            out.points = {{0, 0}, {30, 0}, {0, 30}, {30, 30}};
            out.rough = true;
            out.shift = {-15, -15};
            out.separator = ", ";
            return out;
        }
        case Shape::Triangle:  // triangle.ts / flippedTriangle.ts: h wide and high, the apex up or down
        case Shape::FlippedTriangle: {
            const double h = size.height;
            out.points = shape == Shape::Triangle ? std::vector<Point>{{0, 0}, {h, 0}, {h / 2, -h}}
                                                  : std::vector<Point>{{0, -h}, {h, -h}, {h / 2, 0}};
            out.rough = true;
            out.shift = {-h / 2, h / 2};
            out.group_class = "outer-path";
            out.separator = ", ";
            return out;
        }
        case Shape::Document: {  // waveEdgedRectangle.ts: the bottom edge is 0.8 of a sine cycle of amplitude h / 4
            const double w = size.width, amplitude = size.height / 6, final_h = size.height * 5 / 6;
            out.points = {{-w / 2, final_h / 2}};
            const std::vector<Point> wave = sine_wave_points(-w / 2, final_h / 2, w / 2, final_h / 2, amplitude, 0.8);
            out.points.insert(out.points.end(), wave.begin(), wave.end());
            out.points.push_back({w / 2, -final_h / 2});
            out.points.push_back({-w / 2, -final_h / 2});
            out.rough = true;
            out.shift = {0, -amplitude / 2};
            return out;
        }
        case Shape::Documents: {  // multiWaveEdgedRectangle.ts (neo)
            constexpr double r = 10;  // rectOffset2
            const double w = label.width + 2 * 16, h = label.height + 3 * 12;
            const double amplitude = h / 4, final_h = h + amplitude / 2;
            const double x = -w / 2, y = -final_h / 2;
            const std::vector<Point> wave = sine_wave_points(x - r, y + final_h + r, x + w - r, y + final_h + r, amplitude, 0.8);
            const double last_y = wave.back().y;
            std::vector<Point> outer = {{x - r, y + r}, {x - r, y + final_h + r}};
            outer.insert(outer.end(), wave.begin(), wave.end());
            for (const Point& p : std::vector<Point>{{x + w - r, last_y - r}, {x + w, last_y - r}, {x + w, last_y - 2 * r},
                                                      {x + w + r, last_y - 2 * r}, {x + w + r, y - r}, {x + r, y - r},
                                                      {x + r, y}, {x, y}, {x, y + r}})
                outer.push_back(p);
            const std::vector<Point> inner = {{x, y + r}, {x + w - r, y + r}, {x + w - r, last_y - r}, {x + w, last_y - r}, {x + w, y}, {x, y}};
            out.points = outer;
            out.layers = {{outer, false, false}, {inner, true, false}};
            out.shift = {0, -amplitude / 2};
            return out;
        }
        case Shape::TaggedDocument: {  // taggedWaveEdgedRectangle.ts: uses the node padding, not the neo paddings
            const double w = label.width + 2 * kNodePadding, h = label.height + 2 * kNodePadding;
            const double amplitude = h / 8, tag_w = 0.2 * w, tag_h = 0.2 * h, final_h = h + amplitude;
            std::vector<Point> body = {{-w / 2 - w / 2 * 0.1, final_h / 2}};
            const std::vector<Point> wave = sine_wave_points(-w / 2 - w / 2 * 0.1, final_h / 2, w / 2 + w / 2 * 0.1, final_h / 2, amplitude, 0.8);
            body.insert(body.end(), wave.begin(), wave.end());
            body.push_back({w / 2 + w / 2 * 0.1, -final_h / 2});
            body.push_back({-w / 2 - w / 2 * 0.1, -final_h / 2});
            const double x = -w / 2 + w / 2 * 0.1, y = -final_h / 2 - tag_h * 0.4;
            std::vector<Point> tag = {{x + w - tag_w, (y + h) * 1.3}, {x + w, y + h - tag_h}, {x + w, (y + h) * 0.9}};
            const std::vector<Point> tag_wave = sine_wave_points(x + w, (y + h) * 1.25, x + w - tag_w, (y + h) * 1.3, -h * 0.02, 0.5);
            tag.insert(tag.end(), tag_wave.begin(), tag_wave.end());
            out.points = body;
            out.layers = {{body, true, false}, {tag, false, false}};  // the tag is the group itself, the body nests in it
            out.shift = {0, -amplitude / 2};
            return out;
        }
        case Shape::LinedDocument: {  // linedWaveEdgedRect.ts (neo)
            const double w = label.width + 2 * 16, h = label.height + 2 * 12;
            const double amplitude = h / 4, final_h = h + amplitude, m = w / 2 * 0.1;
            std::vector<Point> pts = {{-w / 2 - m, -final_h / 2}, {-w / 2 - m, final_h / 2}};
            const std::vector<Point> wave = sine_wave_points(-w / 2 - m, final_h / 2, w / 2 + m, final_h / 2, amplitude, 0.8);
            pts.insert(pts.end(), wave.begin(), wave.end());
            for (const Point& p : std::vector<Point>{{w / 2 + m, -final_h / 2}, {-w / 2 - m, -final_h / 2}, {-w / 2, -final_h / 2},
                                                      {-w / 2, final_h / 2 * 1.1}, {-w / 2, -final_h / 2}})
                pts.push_back(p);
            out.points = pts;
            out.layers = {{pts, false, true}};
            out.shift = {0, -amplitude / 2};
            return out;
        }
        default:
            return std::nullopt;
    }
}

Point label_offset(Shape shape, const Size& size, const Size& label) {
    if (shape == Shape::Odd) return {size.height / 8, 0};  // -notch / 2, notch = -h / 4
    // waveEdgedRectangle.ts: the label starts at -w / 2 + padding (15), above the wave; the box is 1.5 x h high
    if (shape == Shape::Document) return {-1, 3 - size.height / 6};
    if (shape == Shape::Documents) return {-10, 10 - (label.height + 3 * 12) / 4};  // -bbox.w / 2 - 10, -bbox.h / 2 + 10 - amplitude
    if (shape == Shape::TaggedDocument) return {0, -(label.height + 2 * kNodePadding) / 16};  // -amplitude / 2, amplitude = h / 8
    if (shape == Shape::LinedDocument) {
        const double w = label.width + 2 * 16, h = label.height + 2 * 12;
        return {-16 + kNodePadding + w / 2 * 0.1 / 2, kNodePadding - 12 - h / 4};
    }
    return {0, 0};
}

std::optional<Point> polygon_border_point(const flowchart::NodeLayout& node, const Point& outside) {
    const std::optional<polygon_outline> outline = polygon_outline_of(node.shape, node.size, node.label);
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

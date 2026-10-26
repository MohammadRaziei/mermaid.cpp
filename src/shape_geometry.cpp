#include "shape_geometry.h"

#include <algorithm>
#include <cmath>

#include <mermaid/macros.h>

#include "rough_path.h"

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

// generateArcPoints: 20 points of the elliptical arc between two points
std::vector<Point> arc_points(double x1, double y1, double x2, double y2, double rx, double ry, bool clockwise) {
    constexpr double pi = 3.14159265358979323846;
    const double mid_x = (x1 + x2) / 2, mid_y = (y1 + y2) / 2, angle = std::atan2(y2 - y1, x2 - x1);
    const double dx = (x2 - x1) / 2, dy = (y2 - y1) / 2;
    const double distance = std::sqrt((dx / rx) * (dx / rx) + (dy / ry) * (dy / ry));
    const double scaled = std::sqrt(MERMAID_MAX(0.0, 1 - distance * distance));
    const double cx = mid_x + scaled * ry * std::sin(angle) * (clockwise ? -1 : 1);
    const double cy = mid_y - scaled * rx * std::cos(angle) * (clockwise ? -1 : 1);
    const double start = std::atan2((y1 - cy) / ry, (x1 - cx) / rx);
    double range = std::atan2((y2 - cy) / ry, (x2 - cx) / rx) - start;
    if (clockwise && range < 0) range += 2 * pi;
    if (!clockwise && range > 0) range -= 2 * pi;
    std::vector<Point> points;
    for (int i = 0; i < 20; ++i) {
        const double a = start + (i / 19.0) * range;
        points.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
    }
    return points;
}

struct arc_segment {
    double rx, ry, rotation;
    bool sweep;
    double dx, dy;
};

// Bounding box of a relative path of arcs (large-arc flag 0) starting at 0,0, as getBBox measures it:
// the endpoint parametrization of the SVG specification (F.6.5), then the extremes of the ellipse.
Rect arc_path_bounds(const std::vector<arc_segment>& segments) {
    constexpr double pi = 3.14159265358979323846;
    double min_x = 0, max_x = 0, min_y = 0, max_y = 0, x0 = 0, y0 = 0;
    const auto include = [&](double x, double y) {
        min_x = MERMAID_MIN(min_x, x); max_x = MERMAID_MAX(max_x, x);
        min_y = MERMAID_MIN(min_y, y); max_y = MERMAID_MAX(max_y, y);
    };
    for (const arc_segment& s : segments) {
        const double x1 = x0, y1 = y0, x2 = x0 + s.dx, y2 = y0 + s.dy;
        const double phi = s.rotation * pi / 180, cp = std::cos(phi), sp = std::sin(phi);
        double rx = std::fabs(s.rx), ry = std::fabs(s.ry);
        const double hx = (x1 - x2) / 2, hy = (y1 - y2) / 2;
        const double x1p = cp * hx + sp * hy, y1p = -sp * hx + cp * hy;
        const double lambda = x1p * x1p / (rx * rx) + y1p * y1p / (ry * ry);
        if (lambda > 1) { rx *= std::sqrt(lambda); ry *= std::sqrt(lambda); }
        const double num = rx * rx * ry * ry - rx * rx * y1p * y1p - ry * ry * x1p * x1p;
        const double den = rx * rx * y1p * y1p + ry * ry * x1p * x1p;
        const double coef = (s.sweep ? 1 : -1) * std::sqrt(MERMAID_MAX(0.0, num / den));  // large-arc 0: the signs differ from the flag
        const double cxp = coef * rx * y1p / ry, cyp = -coef * ry * x1p / rx;
        const double cx = cp * cxp - sp * cyp + (x1 + x2) / 2, cy = sp * cxp + cp * cyp + (y1 + y2) / 2;
        const double theta1 = std::atan2((y1p - cyp) / ry, (x1p - cxp) / rx);
        double delta = std::atan2((-y1p - cyp) / ry, (-x1p - cxp) / rx) - theta1;
        if (!s.sweep && delta > 0) delta -= 2 * pi;
        if (s.sweep && delta < 0) delta += 2 * pi;
        include(x2, y2);
        const double candidates[4] = {std::atan2(-ry * sp, rx * cp), std::atan2(-ry * sp, rx * cp) + pi,
                                      std::atan2(ry * cp, rx * sp), std::atan2(ry * cp, rx * sp) + pi};
        for (double t : candidates) {
            double rel = std::fmod(t - theta1, 2 * pi);
            if (delta >= 0 ? rel < 0 : rel > 0) rel += delta >= 0 ? 2 * pi : -2 * pi;
            if (std::fabs(rel) <= std::fabs(delta))
                include(cx + rx * cp * std::cos(t) - ry * sp * std::sin(t), cy + rx * sp * std::cos(t) + ry * cp * std::sin(t));
        }
        x0 = x2;
        y0 = y2;
    }
    return {min_x, min_y, max_x - min_x, max_y - min_y};
}

// The path text of a template literal: arcs of a group on lines of their own, groups apart by a blank line.
std::string arc_path_text(const std::vector<std::vector<arc_segment>>& groups) {
    std::string d = "M0 0 ";
    for (size_t g = 0; g < groups.size(); ++g) {
        d += g == 0 ? "\n    " : "\n\n    ";
        for (size_t i = 0; i < groups[g].size(); ++i) {
            const arc_segment& s = groups[g][i];
            if (i != 0) d += "\n    ";
            d += "a" + js_number(s.rx) + "," + js_number(s.ry) + " " + js_number(s.rotation) + " 0," + (s.sweep ? "1" : "0") + " " +
                 js_number(s.dx) + "," + js_number(s.dy);
        }
    }
    return d + "\n  H0 V0 Z";
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

std::optional<arc_shape> arc_shape_of(Shape shape, const Size& label) {
    const double half_padding = kNodePadding / 2;
    std::vector<std::vector<arc_segment>> groups;
    Size nominal;
    if (shape == Shape::Cloud) {  // cloud.ts
        const double w = label.width + 2 * half_padding, h = label.height + 2 * half_padding;
        const double r1 = 0.15 * w, r2 = 0.25 * w, r3 = 0.35 * w, r4 = 0.2 * w;
        groups = {{{r1, r1, 0, true, w * 0.25, -1 * w * 0.1}, {r3, r3, 1, true, w * 0.4, -1 * w * 0.1}, {r2, r2, 1, true, w * 0.35, w * 0.2}},
                  {{r1, r1, 1, true, w * 0.15, h * 0.35}, {r4, r4, 1, true, -1 * w * 0.15, h * 0.65}},
                  {{r2, r1, 1, true, -1 * w * 0.25, w * 0.15}, {r3, r3, 1, true, -1 * w * 0.5, 0}, {r1, r1, 1, true, -1 * w * 0.25, -1 * w * 0.15}},
                  {{r1, r1, 1, true, -1 * w * 0.1, -1 * h * 0.35}, {r4, r4, 1, true, w * 0.1, -1 * h * 0.65}}};
        nominal = {w, h};
    } else if (shape == Shape::Bang) {  // bang.ts
        const double w = label.width + 10 * half_padding, h = label.height + 8 * half_padding, r = 0.15 * w;
        const double ew = MERMAID_MAX(w, label.width + 20), eh = MERMAID_MAX(h, label.height + 20);
        groups = {{{r, r, 1, false, ew * 0.25, -1 * eh * 0.1}, {r, r, 1, false, ew * 0.25, 0}, {r, r, 1, false, ew * 0.25, 0}, {r, r, 1, false, ew * 0.25, eh * 0.1}},
                  {{r, r, 1, false, ew * 0.15, eh * 0.33}, {r * 0.8, r * 0.8, 1, false, 0, eh * 0.34}, {r, r, 1, false, -1 * ew * 0.15, eh * 0.33}},
                  {{r, r, 1, false, -1 * ew * 0.25, eh * 0.15}, {r, r, 1, false, -1 * ew * 0.25, 0}, {r, r, 1, false, -1 * ew * 0.25, 0}, {r, r, 1, false, -1 * ew * 0.25, -1 * eh * 0.15}},
                  {{r, r, 1, false, -1 * ew * 0.1, -1 * eh * 0.33}, {r * 0.8, r * 0.8, 1, false, 0, -1 * eh * 0.34}, {r, r, 1, false, ew * 0.1, -1 * eh * 0.33}}};
        nominal = {ew, eh};
    } else {
        return std::nullopt;
    }
    std::vector<arc_segment> all;
    for (const auto& g : groups) all.insert(all.end(), g.begin(), g.end());
    const Rect box = arc_path_bounds(all);
    return arc_shape{arc_path_text(groups), {box.width, box.height}, nominal, {box.x, box.y}};
}

bool writes_tight_label_transform(Shape shape) {
    return shape == Shape::Document || shape == Shape::TaggedDocument || shape == Shape::LinedDocument ||
           shape == Shape::BraceLeft || shape == Shape::BraceRight || shape == Shape::Braces;  // multiWave writes `, `
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
        case Shape::Flag: {  // waveRectangle.ts: one full wave along the bottom and one back along the top
            const double w = label.width + 2 * 16, h = label.height + 20;
            const double amplitude = h / 8, final_h = h + amplitude * 2;
            out.points = {{-w / 2, final_h / 2}};
            const std::vector<Point> bottom = sine_wave_points(-w / 2, final_h / 2, w / 2, final_h / 2, amplitude, 1);
            const std::vector<Point> top = sine_wave_points(w / 2, -final_h / 2, -w / 2, -final_h / 2, amplitude, -1);
            out.points.insert(out.points.end(), bottom.begin(), bottom.end());
            out.points.push_back({w / 2, -final_h / 2});
            out.points.insert(out.points.end(), top.begin(), top.end());
            out.rough = true;
            out.has_shift = false;
            out.group_class = "basic label-container";
            return out;
        }
        case Shape::Delay: {  // halfRoundedRectangle.ts: generateCirclePoints(-w / 2 + r, 0, r, 50, 90, 270) mirrors both axes
            constexpr double pi = 3.14159265358979323846;
            const double h = MERMAID_MAX(10.0, label.height) + 2 * 12, r = h / 2;
            const double clearance = r - std::sqrt(MERMAID_MAX(0.0, r * r - (label.height / 2) * (label.height / 2)));
            const double w = MERMAID_MAX(15.0, label.width) + clearance * 2 + 2 * 16;
            out.points = {{-w / 2, -h / 2}, {w / 2 - r, -h / 2}};
            const double start = 90 * pi / 180, step = (270 * pi / 180 - start) / 49;
            for (int i = 0; i < 50; ++i) {
                const double angle = start + i * step;
                out.points.push_back({-((-w / 2 + r) + r * std::cos(angle)), -(0 + r * std::sin(angle))});
            }
            out.points.push_back({w / 2 - r, h / 2});
            out.points.push_back({-w / 2, h / 2});
            out.rough = true;
            out.has_shift = false;
            return out;
        }
        case Shape::NotchedPentagon: {  // trapezoidalPentagon.ts
            const double w = label.width + 2 * 16, h = label.height + 2 * 12;
            out.points = {{-w / 2 * 0.8, -h / 2}, {w / 2 * 0.8, -h / 2}, {w / 2, -h / 2 * 0.6}, {w / 2, h / 2}, {-w / 2, h / 2}, {-w / 2, -h / 2 * 0.6}};
            out.rough = true;
            out.has_shift = false;
            return out;
        }
        case Shape::SlopedRectangle: {  // slopedRect.ts: drawn in a box of height h, moved down by h / 4
            const double w = label.width + 2 * 16, h = label.height + 2 * 12;
            const double x = -w / 2, y = -h / 2;
            out.points = {{x, y}, {x, y + h}, {x + w, y + h}, {x + w, y - h / 2}};
            out.rough = true;
            out.shift = {0, h / 4};
            out.separator = ", ";
            out.group_class = "basic label-container  outer-path";
            return out;
        }
        case Shape::NotchedRectangle: {  // card.ts: insertPolygonShape, the notch is 12
            const double w = label.width + 2 * 28, h = label.height + 2 * 24;
            out.points = {{12, -h}, {w, -h}, {w, 0}, {0, 0}, {0, -h + 12}, {12, -h}};
            out.anchor = {w, h};
            return out;
        }
        case Shape::LinedRectangle: {  // shadedProcess.ts: rc.polygon, edges meet the box
            constexpr double frame = 8;
            const double total_w = label.width + 2 * 16 + frame, h = label.height + 2 * 12;
            const double w = total_w - frame, x = frame - total_w / 2, y = -h / 2;
            const std::vector<Point> pts = {{x, y}, {x + w, y}, {x + w, y + h}, {x - frame, y + h}, {x - frame, y}, {x, y}, {x, y + h}};
            out.points = pts;
            out.layers = {{pts, false, true}};
            out.has_shift = false;
            out.hit_as_rect = true;
            out.group_style = true;
            return out;
        }
        case Shape::DividedRectangle: {  // dividedRectangle.ts: rc.polygon, edges meet the box
            const double w = label.width + 16, h = label.height + 16, offset = h * 0.2;
            const double x = -w / 2, y = -h / 2 - offset / 2;
            const std::vector<Point> pts = {{x, y + offset}, {-x, y + offset}, {-x, -y}, {x, -y}, {x, y}, {-x, y}, {-x, y + offset}};
            out.points = pts;
            out.layers = {{pts, false, true}};
            out.has_shift = false;
            out.hit_as_rect = true;
            return out;
        }
        case Shape::CurvedTrapezoid: {  // curvedTrapezoid.ts: drawn from the top left corner, then moved by (-w / 2, -h / 2)
            constexpr double pi = 3.14159265358979323846;
            const double h = MERMAID_MAX(5.0, label.height + 2 * 12), r = h / 2;
            const double clearance = r - std::sqrt(MERMAID_MAX(0.0, r * r - (label.height / 2) * (label.height / 2)));
            const double side = MERMAID_MAX(h / 4, clearance);
            const double w = MERMAID_MAX(MERMAID_MAX(20.0, label.width + 2 * 16 + side * 2), (label.width + 2 * 16) * 1.25);
            const double rw = w - r, tw = h / 4;
            out.points = {{rw, 0}, {tw, 0}, {0, h / 2}, {tw, h}, {rw, h}};
            const double start = 270 * pi / 180, step = (90 * pi / 180 - start) / 49;
            for (int i = 0; i < 50; ++i) {  // generateCirclePoints(-rw, -h / 2, r, 50, 270, 90) mirrors both axes
                const double angle = start + i * step;
                out.points.push_back({-(-rw + r * std::cos(angle)), -(-h / 2 + r * std::sin(angle))});
            }
            out.rough = true;
            out.shift = {-w / 2, -h / 2};
            out.separator = ", ";
            return out;
        }
        case Shape::StackedRectangle: {  // multiRect.ts: two outlines, each merged into one path
            constexpr double r = 10;
            const double w = label.width + 2 * 16, h = label.height + 2 * 12, x = -w / 2, y = -h / 2;
            const std::vector<Point> outer = {{x - r, y + r}, {x - r, y + h + r}, {x + w - r, y + h + r}, {x + w - r, y + h}, {x + w, y + h},
                                              {x + w, y + h - r}, {x + w + r, y + h - r}, {x + w + r, y - r}, {x + r, y - r}, {x + r, y},
                                              {x, y}, {x, y + r}};
            const std::vector<Point> inner = {{x, y + r}, {x + w - r, y + r}, {x + w - r, y + h}, {x + w, y + h}, {x + w, y}, {x, y}};
            out.points = outer;
            out.layers = {{outer, true, false, true}, {inner, true, false, true}};
            out.has_shift = false;
            return out;
        }
        case Shape::TaggedRectangle: {  // taggedRect.ts: the box nests in the group of the tag
            const double h = label.height + 2 * 12, tag = 0.2 * h;
            const double w = label.width + 2 * 16, x = -w / 2, y = -h / 2;
            const std::vector<Point> rect = {{x - tag / 2, y}, {x + w + tag / 2, y}, {x + w + tag / 2, y + h}, {x - tag / 2, y + h}};
            const std::vector<Point> tag_points = {{x + w - tag / 2, y + h}, {x + w + tag / 2, y + h}, {x + w + tag / 2, y + h - tag}};
            out.points = rect;
            out.layers = {{rect, true, false, false}, {tag_points, false, false, false}};
            out.has_shift = false;
            return out;
        }
        case Shape::BowTieRectangle: {  // bowTieRect.ts: two inward arcs, the whole drawing moved right by rx / 2
            const double w = label.width + 2 * 16, h = label.height + 12;
            const double ry = h / 2, rx = ry / (2.5 + h / 50);
            out.points = {{w / 2, -h / 2}, {-w / 2, -h / 2}};
            const std::vector<Point> left = arc_points(-w / 2, -h / 2, -w / 2, h / 2, rx, ry, false);
            out.points.insert(out.points.end(), left.begin(), left.end());
            out.points.push_back({w / 2, h / 2});
            const std::vector<Point> right = arc_points(w / 2, h / 2, w / 2, -h / 2, rx, ry, true);
            out.points.insert(out.points.end(), right.begin(), right.end());
            out.rough = true;
            out.shift = {rx / 2, 0};
            out.separator = ", ";
            return out;
        }
        case Shape::LightningBolt: {  // lightningBolt.ts: drawn from 0,0 (35 x 70), moved by translate(-17.5,-35); no label
            constexpr double width = 35, height = 35, gap = 7;
            out.points = {{width, 0}, {0, height + gap / 2}, {width - 2 * gap, height + gap / 2}, {0, 2 * height}, {width, height - gap / 2}, {2 * gap, height - gap / 2}};
            out.rough = true;
            out.shift = {-width / 2, -height};
            out.separator = ",";
            out.group_class = "outer-path";
            return out;
        }
        case Shape::BraceLeft:
        case Shape::BraceRight:
        case Shape::Braces: {  // curlyBraceLeft.ts, curlyBraceRight.ts, curlyBraces.ts: open outlines with no fill, and an invisible box
            const bool left = shape == Shape::BraceLeft;  // the left brace pads by 18 x 12 once, the others twice
            const double w = label.width + (left ? 18 : 2 * 18), h = label.height + (left ? 12 : 2 * 12);
            const double r = MERMAID_MAX(5.0, h * 0.1);
            const double sign = shape == Shape::BraceRight ? 1 : -1;  // curlyBraceRight.ts uses the variant that does not mirror
            const auto gcp = [sign](double cx, double cy, double radius, int n, double from, double to) {  // generateCirclePoints
                constexpr double pi = 3.14159265358979323846;
                std::vector<Point> pts;
                const double start = from * pi / 180, step = (to * pi / 180 - start) / (n - 1);
                for (int i = 0; i < n; ++i) {
                    const double a = start + i * step;
                    pts.push_back({sign * (cx + radius * std::cos(a)), sign * (cy + radius * std::sin(a))});
                }
                return pts;
            };
            const auto join = [](std::vector<std::vector<Point>> parts) {
                std::vector<Point> all;
                for (const auto& part : parts) all.insert(all.end(), part.begin(), part.end());
                return all;
            };
            const auto left_brace = [&](int top_count) {
                return join({gcp(w / 2, -h / 2, r, top_count, -90, 0), {{-w / 2 - r, r}}, gcp(w / 2 + r * 2, -r, r, 20, -180, -270),
                             gcp(w / 2 + r * 2, r, r, 20, -90, -180), {{-w / 2 - r, -h / 2}}, gcp(w / 2, h / 2, r, 20, 0, 90)});
            };
            const auto right_brace = [&]() {
                return join({gcp(w / 2, -h / 2, r, 20, -90, 0), {{w / 2 + r, -r}}, gcp(w / 2 + r * 2, -r, r, 20, -180, -270),
                             gcp(w / 2 + r * 2, r, r, 20, -90, -180), {{w / 2 + r, h / 2}}, gcp(w / 2, h / 2, r, 20, 0, 90)});
            };
            const auto inner_right_brace = [&]() {  // the right half of `braces`
                return join({gcp(-w / 2 + r + r / 2, -h / 2, r, 20, -90, -180), {{w / 2 - r / 2, r}}, gcp(-w / 2 - r / 2, -r, r, 20, 0, 90),
                             gcp(-w / 2 - r / 2, r, r, 20, -90, 0), {{w / 2 - r / 2, -r}}, gcp(-w / 2 + r + r / 2, h / 2, r, 30, -180, -270)});
            };
            const auto stroke_layer = [](std::vector<Point> pts, bool open, bool hidden) {
                polygon_outline::layer l;
                l.points = std::move(pts);
                l.nested = true;
                l.stroke_only = true;
                l.open = open;
                l.hidden = hidden;
                return l;
            };
            std::vector<Point> rect;
            if (shape == Shape::BraceLeft) {
                rect = join({{{w / 2, -h / 2 - r}, {-w / 2, -h / 2 - r}}, gcp(w / 2, -h / 2, r, 20, -90, 0), {{-w / 2 - r, -r}},
                             gcp(w / 2 + w * 0.1, -r, r, 20, -180, -270), gcp(w / 2 + w * 0.1, r, r, 20, -90, -180), {{-w / 2 - r, h / 2}},
                             gcp(w / 2, h / 2, r, 20, 0, 90), {{-w / 2, h / 2 + r}, {w / 2, h / 2 + r}}});
                out.layers = {stroke_layer(left_brace(30), true, false), stroke_layer(rect, false, true)};
                out.shift = {r, 0};
            } else if (shape == Shape::BraceRight) {
                rect = join({{{-w / 2, -h / 2 - r}, {w / 2, -h / 2 - r}}, gcp(w / 2, -h / 2, r, 20, -90, 0), {{w / 2 + r, -r}},
                             gcp(w / 2 + r * 2, -r, r, 20, -180, -270), gcp(w / 2 + r * 2, r, r, 20, -90, -180), {{w / 2 + r, h / 2}},
                             gcp(w / 2, h / 2, r, 20, 0, 90), {{w / 2, h / 2 + r}, {-w / 2, h / 2 + r}}});
                out.layers = {stroke_layer(right_brace(), true, false), stroke_layer(rect, false, true)};
                out.shift = {-r, 0};
            } else {
                rect = join({{{w / 2, -h / 2 - r}, {-w / 2, -h / 2 - r}}, gcp(w / 2, -h / 2, r, 20, -90, 0), {{-w / 2 - r, -r}},
                             gcp(w / 2 + r * 2, -r, r, 20, -180, -270), gcp(w / 2 + r * 2, r, r, 20, -90, -180), {{-w / 2 - r, h / 2}},
                             gcp(w / 2, h / 2, r, 20, 0, 90), {{-w / 2, h / 2 + r}, {w / 2 - r - r / 2, h / 2 + r}}, inner_right_brace()});
                out.layers = {stroke_layer(inner_right_brace(), true, false), stroke_layer(left_brace(30), true, false), stroke_layer(rect, false, true)};
                out.shift = {r - r / 4, 0};
            }
            out.points = rect;
            out.separator = ", ";
            out.group_class = "text";
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
    if (shape == Shape::SlopedRectangle) {  // -w / 2 + padding, -h / 4 + padding: from the centred label
        const double h = label.height + 2 * 12;
        return {-16 + kNodePadding, -h / 4 + kNodePadding + label.height / 2};
    }
    if (shape == Shape::TiltedCylinder) {  // -bbox.width / 2 - rx
        const double h = label.height + 12;
        return {-(h / 2) / (2.5 + h / 50), 0};
    }
    if (shape == Shape::LinedCylinder) {  // -bbox.height / 2 + ry
        const double w = label.width + 2 * 16;
        return {0, (w / 2) / (2.5 + w / 50)};
    }
    if (shape == Shape::BraceLeft) {  // translate(-w / 2 + radius, -h / 2 + padding / 2), padded by 18 x 12 once
        const double h = label.height + 12, r = MERMAID_MAX(5.0, h * 0.1);
        return {r - 9, -6 + kNodePadding / 2};
    }
    if (shape == Shape::BraceRight || shape == Shape::Braces) return {-18 + kNodePadding / 2, -12 + kNodePadding / 2};
    if (shape == Shape::StackedRectangle) return {-10, 10};  // -rectOffset, +rectOffset
    if (shape == Shape::LinedRectangle) return {8.0 / 2, 0};  // FRAME_WIDTH / 2 - bbox.width / 2
    if (shape == Shape::DividedRectangle) return {-0.5, (label.height + 16) * 0.1 - 0.5};  // padding 15 / 2 against the 16 of the box
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
    if (!outline || outline->hit_as_rect) return std::nullopt;

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

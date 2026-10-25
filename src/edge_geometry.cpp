#include <mermaid/edge_geometry.h>

#include <mermaid/macros.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>

#include "shape_geometry.h"
#include "svg_writer.h"

using mermaid::detail::SvgWriter;

namespace mermaid {

namespace {

using flowchart::NodeLayout;
using flowchart::Shape;

constexpr double kEpsilon = 1e-5;
constexpr double kPi = 3.14159265358979323846;  // M_PI is not portable

// Segments of the rounded path: a line to `to`, or a quadratic curve through `control` to `to`.
struct piece {
    bool curve = false;
    Point from;
    Point control;
    Point to;
};

struct corner {
    bool rounded = false;
    Point start;
    Point end;
};

corner round_corner(const Point& prev, const Point& cur, const Point& next, double radius) {
    const double dx1 = cur.x - prev.x, dy1 = cur.y - prev.y;
    const double dx2 = next.x - cur.x, dy2 = next.y - cur.y;
    const double len1 = std::hypot(dx1, dy1), len2 = std::hypot(dx2, dy2);
    if (len1 < kEpsilon || len2 < kEpsilon) return {};
    const double nx1 = dx1 / len1, ny1 = dy1 / len1, nx2 = dx2 / len2, ny2 = dy2 / len2;
    const double dot = MERMAID_MAX(-1.0, MERMAID_MIN(1.0, nx1 * nx2 + ny1 * ny2));
    const double angle = std::acos(dot);
    if (angle < kEpsilon || std::fabs(kPi - angle) < kEpsilon) return {};
    const double cut = MERMAID_MIN(MERMAID_MIN(radius / std::sin(angle / 2), len1 / 2), len2 / 2);
    return {true, {cur.x - nx1 * cut, cur.y - ny1 * cut}, {cur.x + nx2 * cut, cur.y + ny2 * cut}};
}

std::vector<piece> rounded_pieces(const std::vector<Point>& pts, double radius) {
    std::vector<piece> out;
    if (pts.size() < 2) return out;
    Point at = pts.front();
    for (size_t i = 1; i < pts.size(); ++i) {
        if (i + 1 == pts.size()) {
            out.push_back({false, at, {}, pts[i]});
            break;
        }
        const corner c = round_corner(pts[i - 1], pts[i], pts[i + 1], radius);
        if (!c.rounded) {
            out.push_back({false, at, {}, pts[i]});
            at = pts[i];
            continue;
        }
        out.push_back({false, at, {}, c.start});
        out.push_back({true, c.start, pts[i], c.end});
        at = c.end;
    }
    return out;
}

// JavaScript's Number -> string for the doubles we print: shortest form that round-trips.
std::string js_number(double v) {
    if (v == 0) return "0";
    char buf[40];
    const auto res = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, res.ptr);
}

std::string base64(const std::string& in) {
    static const char* const table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const unsigned n = (static_cast<unsigned char>(in[i]) << 16) | (static_cast<unsigned char>(in[i + 1]) << 8) | static_cast<unsigned char>(in[i + 2]);
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += table[(n >> 6) & 63];
        out += table[n & 63];
    }
    if (i + 1 == in.size()) {
        const unsigned n = static_cast<unsigned char>(in[i]) << 16;
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += "==";
    } else if (i + 2 == in.size()) {
        const unsigned n = (static_cast<unsigned char>(in[i]) << 16) | (static_cast<unsigned char>(in[i + 1]) << 8);
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += table[(n >> 6) & 63];
        out += '=';
    }
    return out;
}

}  // namespace

Point edge_geometry::border_point(const NodeLayout& node, const Point& outside) {
    const double dx = outside.x - node.center.x, dy = outside.y - node.center.y;
    const double w = node.size.width / 2, h = node.size.height / 2;
    if (dx == 0 && dy == 0) return node.center;
    switch (node.shape) {
        case Shape::Diamond: {
            const double t = 1 / (std::fabs(dx) / w + std::fabs(dy) / h);
            return {node.center.x + t * dx, node.center.y + t * dy};
        }
        case Shape::Circle:
        case Shape::SmallCircle:
        case Shape::DoubleCircle: {
            const double r = MERMAID_MIN(w, h), len = std::hypot(dx, dy);
            return {node.center.x + r * dx / len, node.center.y + r * dy / len};
        }
        default: {
            if (const auto polygon = detail::polygon_border_point(node, outside)) return *polygon;
            // dagre's intersect-rect
            double sx, sy;
            if (std::fabs(dy) * w > std::fabs(dx) * h) {
                sy = dy < 0 ? -h : h;
                sx = dy == 0 ? 0 : sy * dx / dy;
            } else {
                sx = dx < 0 ? -w : w;
                sy = dx == 0 ? 0 : sx * dy / dx;
            }
            return {node.center.x + sx, node.center.y + sy};
        }
    }
}

namespace {

double js_round(double v) { return std::floor(v + 0.5); }  // Math.round: halves go up

double round_to(double v, int decimals) {
    const double factor = std::pow(10.0, decimals);
    return js_round(v * factor) / factor;
}

}  // namespace

Point edge_geometry::label_position(const std::vector<Point>& points) {
    if (points.size() == 1) return points.front();
    double total = 0;
    for (size_t i = 1; i < points.size(); ++i) total += std::hypot(points[i].x - points[i - 1].x, points[i].y - points[i - 1].y);

    // calculatePoint: walk along the polyline until the remaining distance falls inside a segment
    double remaining = total / 2;
    for (size_t i = 1; i < points.size(); ++i) {
        const Point& prev = points[i - 1];
        const Point& point = points[i];
        const double length = std::hypot(point.x - prev.x, point.y - prev.y);
        if (length == 0) return prev;
        if (length < remaining) {
            remaining -= length;
            continue;
        }
        const double ratio = remaining / length;
        if (ratio <= 0) return prev;
        if (ratio >= 1) return point;
        return {round_to((1 - ratio) * prev.x + ratio * point.x, 5), round_to((1 - ratio) * prev.y + ratio * point.y, 5)};
    }
    return points.back();
}

bool edge_geometry::is_label_coordinate_in_path(const Point& point, const std::string& path) {
    // replace(/(\d+\.\d+)/g, n => Math.round(parseFloat(n)).toString())
    std::string sanitized;
    for (size_t i = 0; i < path.size();) {
        if (!std::isdigit(static_cast<unsigned char>(path[i]))) {
            sanitized += path[i++];
            continue;
        }
        size_t j = i;
        while (j < path.size() && std::isdigit(static_cast<unsigned char>(path[j]))) ++j;
        if (j + 1 < path.size() && path[j] == '.' && std::isdigit(static_cast<unsigned char>(path[j + 1]))) {
            size_t k = j + 1;
            while (k < path.size() && std::isdigit(static_cast<unsigned char>(path[k]))) ++k;
            sanitized += std::to_string(static_cast<long long>(js_round(std::strtod(path.substr(i, k - i).c_str(), nullptr))));
            i = k;
        } else {
            sanitized += path.substr(i, j - i);
            i = j;
        }
    }
    const auto text = [](double v) { return std::to_string(static_cast<long long>(js_round(v))); };
    return sanitized.find(text(point.x)) != std::string::npos || sanitized.find(text(point.y)) != std::string::npos;
}

std::vector<Point> edge_geometry::clip_to_nodes(const std::vector<Point>& route, const NodeLayout& tail, const NodeLayout& head) {
    std::vector<Point> inner;
    if (route.size() > 2) inner.assign(route.begin() + 1, route.end() - 1);
    if (inner.empty()) return {border_point(tail, head.center), border_point(head, tail.center)};
    std::vector<Point> out;
    out.push_back(border_point(tail, inner.front()));
    out.insert(out.end(), inner.begin(), inner.end());
    out.push_back(border_point(head, inner.back()));
    return out;
}

namespace {

// edges.js `outsideNode` (note the >=: a point on the border is outside)
bool outside_box(const Rect& node, const Point& point) {
    const Point c = node.center();
    return std::fabs(point.x - c.x) >= node.width / 2 || std::fabs(point.y - c.y) >= node.height / 2;
}

// intersect.js `intersection(node, outsidePoint, insidePoint)`, kept line by line (including its asymmetries)
Point box_intersection(const Rect& node, const Point& outside, const Point& inside) {
    const Point c = node.center();
    const double x = c.x, y = c.y;
    const double dx = std::fabs(x - inside.x);
    const double w = node.width / 2;
    double r = inside.x < outside.x ? w - dx : w + dx;
    const double h = node.height / 2;
    const double q_total = std::fabs(outside.y - inside.y);
    const double r_total = std::fabs(outside.x - inside.x);
    if (std::fabs(y - outside.y) * w > std::fabs(x - outside.x) * h) {  // the top or the bottom
        const double q = inside.y < outside.y ? outside.y - h - y : y - h - outside.y;
        r = r_total * q / q_total;
        Point res{inside.x < outside.x ? inside.x + r : inside.x - r_total + r,
                  inside.y < outside.y ? inside.y + q_total - q : inside.y - q_total + q};
        if (r == 0) res = outside;
        if (r_total == 0) res.x = outside.x;
        if (q_total == 0) res.y = outside.y;
        return res;
    }
    r = inside.x < outside.x ? outside.x - w - x : x - w - outside.x;  // the sides
    const double q = q_total * r / r_total;
    Point res{inside.x < outside.x ? inside.x + r_total - r : inside.x - r_total + r, inside.y < outside.y ? inside.y + q : inside.y - q};
    if (r == 0) res = outside;
    if (r_total == 0) res.x = outside.x;
    if (q_total == 0) res.y = outside.y;
    return res;
}

}  // namespace

std::vector<Point> edge_geometry::cut_path_at_intersect(const std::vector<Point>& points, const Rect& boundary) {
    std::vector<Point> out;
    if (points.empty()) return out;
    Point last_outside = points.front();
    bool inside = false;
    for (const Point& p : points) {
        if (!outside_box(boundary, p) && !inside) {
            const Point at = box_intersection(boundary, last_outside, p);
            const bool present = std::any_of(out.begin(), out.end(), [&](const Point& q) { return q.x == at.x && q.y == at.y; });
            if (!present) out.push_back(at);
            inside = true;
        } else {
            last_outside = p;
            if (!inside) out.push_back(p);
        }
    }
    return out;
}

std::vector<Point> edge_geometry::shorten_ends(const std::vector<Point>& points, double start, double end) {
    std::vector<Point> out = points;
    const size_t n = points.size();
    if (n < 2) return out;
    if (start != 0) {
        const double angle = std::atan2(points[1].y - points[0].y, points[1].x - points[0].x);
        out[0] = {points[0].x + start * std::cos(angle), points[0].y + start * std::sin(angle)};
    }
    if (end != 0) {
        const double angle = std::atan2(points[n - 1].y - points[n - 2].y, points[n - 1].x - points[n - 2].x);
        out[n - 1] = {points[n - 1].x - end * std::cos(angle), points[n - 1].y - end * std::sin(angle)};
    }
    return out;
}

std::string edge_geometry::rounded_path(const std::vector<Point>& points, double radius) {
    if (points.size() < 2) return {};
    auto pt = [](const Point& p) { return SvgWriter::num(p.x) + "," + SvgWriter::num(p.y); };
    std::string d = "M" + pt(points.front());
    for (const piece& p : rounded_pieces(points, radius))
        d += p.curve ? "Q" + pt(p.control) + " " + pt(p.to) : "L" + pt(p.to);
    return d;
}

double edge_geometry::rounded_length(const std::vector<Point>& points, double radius) {
    double total = 0;
    for (const piece& p : rounded_pieces(points, radius)) {
        if (!p.curve) {
            total += std::hypot(p.to.x - p.from.x, p.to.y - p.from.y);
            continue;
        }
        Point last = p.from;
        constexpr int steps = 200;
        for (int i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps, u = 1 - t;
            const Point now{u * u * p.from.x + 2 * u * t * p.control.x + t * t * p.to.x,
                            u * u * p.from.y + 2 * u * t * p.control.y + t * t * p.to.y};
            total += std::hypot(now.x - last.x, now.y - last.y);
            last = now;
        }
    }
    return total;
}

namespace {

// A d3 curve as pieces: the first point, then lines and cubic curves.
struct curve_piece {
    bool cubic = false;
    Point c1, c2, to;
};

curve_piece line_to(const Point& to) { return {false, {}, {}, to}; }

std::vector<curve_piece> linear_pieces(const std::vector<Point>& p) {
    std::vector<curve_piece> out;
    for (size_t i = 1; i < p.size(); ++i) out.push_back(line_to(p[i]));
    return out;
}

// curveBasis
std::vector<curve_piece> basis_pieces(const std::vector<Point>& p) {
    if (p.size() < 3) return linear_pieces(p);
    std::vector<curve_piece> out;
    auto bezier = [&](const Point& a, const Point& b, const Point& c) {
        out.push_back({true,
                       {(2 * a.x + b.x) / 3, (2 * a.y + b.y) / 3},
                       {(a.x + 2 * b.x) / 3, (a.y + 2 * b.y) / 3},
                       {(a.x + 4 * b.x + c.x) / 6, (a.y + 4 * b.y + c.y) / 6}});
    };
    out.push_back(line_to({(5 * p[0].x + p[1].x) / 6, (5 * p[0].y + p[1].y) / 6}));
    for (size_t i = 2; i < p.size(); ++i) bezier(p[i - 2], p[i - 1], p[i]);
    bezier(p[p.size() - 2], p.back(), p.back());
    out.push_back(line_to(p.back()));
    return out;
}

// curveCardinal with d3's default tension 0 (k = 1/6); the first and last tangents are zero.
std::vector<curve_piece> cardinal_pieces(const std::vector<Point>& p) {
    if (p.size() < 3) return linear_pieces(p);
    constexpr double k = 1.0 / 6.0;
    std::vector<curve_piece> out;
    // segment p[i] -> p[i+1] with the neighbours before and after
    auto segment = [&](const Point& before, const Point& from, const Point& to, const Point& after) {
        out.push_back({true,
                       {from.x + k * (to.x - before.x), from.y + k * (to.y - before.y)},
                       {to.x + k * (from.x - after.x), to.y + k * (from.y - after.y)},
                       to});
    };
    segment(p[1], p[0], p[1], p[2]);
    for (size_t i = 1; i + 2 < p.size(); ++i) segment(p[i - 1], p[i], p[i + 1], p[i + 2]);
    segment(p[p.size() - 3], p[p.size() - 2], p.back(), p[p.size() - 2]);  // d3's lineEnd repeats the penultimate point
    return out;
}

// curveStep, curveStepAfter (t = 1) and curveStepBefore (t = 0): d3's step with the turn at fraction `t` of the way.
std::vector<curve_piece> step_pieces(const std::vector<Point>& p, double t) {
    std::vector<curve_piece> out;
    for (size_t i = 1; i < p.size(); ++i) {
        const Point& from = p[i - 1];
        const Point& to = p[i];
        if (t <= 0) {  // d3: the vertical leg comes first, at the old x
            out.push_back(line_to({from.x, to.y}));
            out.push_back(line_to(to));
            continue;
        }
        const double x = from.x * (1 - t) + to.x * t;
        out.push_back(line_to({x, from.y}));
        out.push_back(line_to({x, to.y}));
    }
    if (p.size() >= 2 && t > 0 && t < 1) out.push_back(line_to(p.back()));
    return out;
}

// curveBumpX / curveBumpY: one cubic per segment, leaving and entering horizontally (or vertically).
std::vector<curve_piece> bump_pieces(const std::vector<Point>& p, bool horizontal) {
    std::vector<curve_piece> out;
    for (size_t i = 1; i < p.size(); ++i) {
        const Point &a = p[i - 1], &b = p[i];
        if (horizontal) {
            const double mid = (a.x + b.x) / 2;
            out.push_back({true, {mid, a.y}, {mid, b.y}, b});
        } else {
            const double mid = (a.y + b.y) / 2;
            out.push_back({true, {a.x, mid}, {b.x, mid}, b});
        }
    }
    return out;
}

// d3's natural spline control points for one coordinate.
std::pair<std::vector<double>, std::vector<double>> natural_controls(const std::vector<double>& x) {
    const size_t n = x.size() - 1;
    std::vector<double> a(n), b(n), r(n);
    a[0] = 0;
    b[0] = 2;
    r[0] = x[0] + 2 * x[1];
    for (size_t i = 1; i + 1 < n; ++i) {
        a[i] = 1;
        b[i] = 4;
        r[i] = 4 * x[i] + 2 * x[i + 1];
    }
    a[n - 1] = 2;
    b[n - 1] = 7;
    r[n - 1] = 8 * x[n - 1] + x[n];
    for (size_t i = 1; i < n; ++i) {
        const double m = a[i] / b[i - 1];
        b[i] -= m;
        r[i] -= m * r[i - 1];
    }
    a[n - 1] = r[n - 1] / b[n - 1];
    for (size_t i = n - 1; i-- > 0;) a[i] = (r[i] - a[i + 1]) / b[i];
    b[n - 1] = (x[n] + a[n - 1]) / 2;
    for (size_t i = 0; i + 1 < n; ++i) b[i] = 2 * x[i + 1] - a[i + 1];
    return {a, b};
}

std::vector<curve_piece> natural_pieces(const std::vector<Point>& p) {
    if (p.size() < 3) return linear_pieces(p);
    std::vector<double> xs, ys;
    for (const Point& q : p) {
        xs.push_back(q.x);
        ys.push_back(q.y);
    }
    const auto px = natural_controls(xs), py = natural_controls(ys);
    std::vector<curve_piece> out;
    for (size_t i = 0; i + 1 < p.size(); ++i)
        out.push_back({true, {px.first[i], py.first[i]}, {px.second[i], py.second[i]}, p[i + 1]});
    return out;
}

// curveMonotoneX (d3-shape monotone.js); the Y variant runs it on swapped coordinates.
int sign_of(double v) { return v < 0 ? -1 : 1; }

// Math.min of two numbers: NaN wins.
double js_min(double a, double b) { return std::isnan(a) || std::isnan(b) ? std::nan("") : MERMAID_MIN(a, b); }

std::vector<curve_piece> monotone_x_pieces(const std::vector<Point>& in) {
    std::vector<Point> p;  // d3 skips a point equal to the previous one
    for (const Point& q : in)
        if (p.empty() || q.x != p.back().x || q.y != p.back().y) p.push_back(q);
    if (p.size() < 3) return linear_pieces(p);
    const auto slope3 = [&](const Point& a, const Point& b, const Point& c) {
        const double h0 = b.x - a.x, h1 = c.x - b.x;
        const double s0 = (b.y - a.y) / (h0 != 0 ? h0 : (h1 < 0 ? -0.0 : 0.0));
        const double s1 = (c.y - b.y) / (h1 != 0 ? h1 : (h0 < 0 ? -0.0 : 0.0));
        const double q = (s0 * h1 + s1 * h0) / (h0 + h1);
        const double smallest = js_min(js_min(std::fabs(s0), std::fabs(s1)), 0.5 * std::fabs(q));
        const double v = (sign_of(s0) + sign_of(s1)) * smallest;
        return std::isnan(v) ? 0.0 : v;  // JavaScript's `|| 0`
    };
    const auto slope2 = [](const Point& a, const Point& b, double t) {
        const double h = b.x - a.x;
        return h != 0 ? (3 * (b.y - a.y) / h - t) / 2 : t;
    };
    const auto bezier = [&](const Point& a, const Point& b, double t0, double t1) {
        const double dx = (b.x - a.x) / 3;
        return curve_piece{true, {a.x + dx, a.y + dx * t0}, {b.x - dx, b.y - dx * t1}, b};
    };
    std::vector<curve_piece> out;
    double t0 = slope3(p[0], p[1], p[2]);
    out.push_back(bezier(p[0], p[1], slope2(p[0], p[1], t0), t0));
    for (size_t i = 2; i < p.size(); ++i) {
        const double t1 = i + 1 < p.size() ? slope3(p[i - 1], p[i], p[i + 1]) : slope2(p[i - 1], p[i], t0);
        out.push_back(bezier(p[i - 1], p[i], t0, t1));
        t0 = t1;
    }
    return out;
}

std::vector<curve_piece> monotone_y_pieces(const std::vector<Point>& p) {
    std::vector<Point> swapped;
    for (const Point& q : p) swapped.push_back({q.y, q.x});
    std::vector<curve_piece> out = monotone_x_pieces(swapped);
    for (curve_piece& piece : out) {
        std::swap(piece.c1.x, piece.c1.y);
        std::swap(piece.c2.x, piece.c2.y);
        std::swap(piece.to.x, piece.to.y);
    }
    return out;
}

// curveCatmullRom with d3's default alpha 0.5.
std::vector<curve_piece> catmull_rom_pieces(const std::vector<Point>& p) {
    if (p.size() < 3) return linear_pieces(p);
    constexpr double alpha = 0.5, epsilon = 1e-12;
    const auto dist_2a = [&](const Point& a, const Point& b) {
        return std::pow((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y), alpha);
    };
    std::vector<curve_piece> out;
    // segment p1 -> p2 with p0 before and p3 after
    const auto segment = [&](const Point& p0, const Point& p1, const Point& p2, const Point& p3) {
        const double l01_2a = dist_2a(p0, p1), l12_2a = dist_2a(p1, p2), l23_2a = dist_2a(p2, p3);
        const double l01_a = std::sqrt(l01_2a), l12_a = std::sqrt(l12_2a), l23_a = std::sqrt(l23_2a);
        Point c1 = p1, c2 = p2;
        if (l01_a > epsilon) {
            const double a = 2 * l01_2a + 3 * l01_a * l12_a + l12_2a, n = 3 * l01_a * (l01_a + l12_a);
            c1 = {(p1.x * a - p0.x * l12_2a + p2.x * l01_2a) / n, (p1.y * a - p0.y * l12_2a + p2.y * l01_2a) / n};
        }
        if (l23_a > epsilon) {
            const double b = 2 * l23_2a + 3 * l23_a * l12_a + l12_2a, m = 3 * l23_a * (l23_a + l12_a);
            c2 = {(p2.x * b + p1.x * l23_2a - p3.x * l12_2a) / m, (p2.y * b + p1.y * l23_2a - p3.y * l12_2a) / m};
        }
        out.push_back({true, c1, c2, p2});
    };
    // d3 starts with the first point standing in for the missing neighbour (all lengths zero there)
    segment(p[0], p[0], p[1], p[2]);
    for (size_t i = 1; i + 2 < p.size(); ++i) segment(p[i - 1], p[i], p[i + 1], p[i + 2]);
    segment(p[p.size() - 3], p[p.size() - 2], p.back(), p.back());
    return out;
}

std::vector<curve_piece> curve_pieces(const std::string& curve, const std::vector<Point>& p) {
    if (curve == "linear") return linear_pieces(p);
    if (curve == "cardinal") return cardinal_pieces(p);
    if (curve == "step") return step_pieces(p, 0.5);
    if (curve == "stepAfter") return step_pieces(p, 1);
    if (curve == "stepBefore") return step_pieces(p, 0);
    if (curve == "bumpX") return bump_pieces(p, true);
    if (curve == "bumpY") return bump_pieces(p, false);
    if (curve == "natural") return natural_pieces(p);
    if (curve == "monotoneX") return monotone_x_pieces(p);
    if (curve == "monotoneY") return monotone_y_pieces(p);
    if (curve == "catmullRom") return catmull_rom_pieces(p);
    return basis_pieces(p);  // Mermaid's default, and what unknown names resolve to here
}

double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

std::string xy(const Point& p) { return SvgWriter::num(round3(p.x)) + "," + SvgWriter::num(round3(p.y)); }

}  // namespace

std::string edge_geometry::curve_path(const std::string& curve, const std::vector<Point>& points) {
    if (points.empty()) return {};
    std::string d = "M" + xy(points.front());
    for (const curve_piece& piece : curve_pieces(curve, points))
        d += piece.cubic ? "C" + xy(piece.c1) + "," + xy(piece.c2) + "," + xy(piece.to) : "L" + xy(piece.to);
    return d;
}

Rect edge_geometry::curve_bounds(const std::string& curve, const std::vector<Point>& points) {
    if (points.empty()) return {};
    double min_x = points.front().x, max_x = min_x, min_y = points.front().y, max_y = min_y;
    const auto take = [&](const Point& p) {
        min_x = std::min(min_x, p.x);
        max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y);
        max_y = std::max(max_y, p.y);
    };
    Point at = points.front();
    for (const curve_piece& piece : curve_pieces(curve, points)) {
        if (piece.cubic) {
            constexpr int kSteps = 256;
            for (int i = 1; i < kSteps; ++i) {
                const double t = static_cast<double>(i) / kSteps, u = 1 - t;
                take({u * u * u * at.x + 3 * u * u * t * piece.c1.x + 3 * u * t * t * piece.c2.x + t * t * t * piece.to.x,
                      u * u * u * at.y + 3 * u * u * t * piece.c1.y + 3 * u * t * t * piece.c2.y + t * t * t * piece.to.y});
            }
        }
        take(piece.to);
        at = piece.to;
    }
    return {min_x, min_y, max_x - min_x, max_y - min_y};
}

double edge_geometry::path_offset(flowchart::Head head) { return head == flowchart::Head::Arrow ? 4 : 0; }

double edge_geometry::curve_length(const std::string& curve, const std::vector<Point>& points) {
    if (points.size() < 2) return 0;
    double total = 0;
    Point at = points.front();
    for (const curve_piece& piece : curve_pieces(curve, points)) {
        if (!piece.cubic) {
            total += std::hypot(piece.to.x - at.x, piece.to.y - at.y);
        } else {
            Point last = at;
            constexpr int kSteps = 256;
            for (int i = 1; i <= kSteps; ++i) {
                const double t = static_cast<double>(i) / kSteps, u = 1 - t;
                const Point q{u * u * u * at.x + 3 * u * u * t * piece.c1.x + 3 * u * t * t * piece.c2.x + t * t * t * piece.to.x,
                              u * u * u * at.y + 3 * u * u * t * piece.c1.y + 3 * u * t * t * piece.c2.y + t * t * t * piece.to.y};
                total += std::hypot(q.x - last.x, q.y - last.y);
                last = q;
            }
        }
        at = piece.to;
    }
    return total;
}

std::vector<Point> edge_geometry::fix_corners(const std::vector<Point>& pts) {
    std::vector<bool> corner(pts.size(), false);
    for (size_t i = 1; i + 1 < pts.size(); ++i) {
        const Point &prev = pts[i - 1], &cur = pts[i], &next = pts[i + 1];
        if ((prev.x == cur.x && cur.y == next.y && std::fabs(cur.x - next.x) > 5 && std::fabs(cur.y - prev.y) > 5) ||
            (prev.y == cur.y && cur.x == next.x && std::fabs(cur.x - prev.x) > 5 && std::fabs(cur.y - next.y) > 5))
            corner[i] = true;
    }
    auto adjacent = [](const Point& a, const Point& b, double distance) {
        const double dx = b.x - a.x, dy = b.y - a.y, ratio = distance / std::hypot(dx, dy);
        return Point{b.x - ratio * dx, b.y - ratio * dy};
    };
    std::vector<Point> out;
    for (size_t i = 0; i < pts.size(); ++i) {
        if (!corner[i]) {
            out.push_back(pts[i]);
            continue;
        }
        const Point &prev = pts[i - 1], &cur = pts[i], &next = pts[i + 1];
        const Point np = adjacent(prev, cur, 5), nn = adjacent(next, cur, 5);
        const double xd = nn.x - np.x, yd = nn.y - np.y, a = std::sqrt(2.0) * 2;
        Point nc = cur;
        if (std::fabs(next.x - prev.x) > 10 && std::fabs(next.y - prev.y) >= 10) {
            const double r = 5;
            if (cur.x == np.x) nc = {xd < 0 ? np.x - r + a : np.x + r - a, yd < 0 ? np.y - a : np.y + a};
            else nc = {xd < 0 ? np.x - a : np.x + a, yd < 0 ? np.y - r + a : np.y + r - a};
        }
        out.push_back(np);
        out.push_back(nc);
        out.push_back(nn);
    }
    return out;
}

std::string edge_geometry::points_attribute(const std::vector<Point>& points) {
    std::string json = "[";
    for (size_t i = 0; i < points.size(); ++i)
        json += std::string(i ? "," : "") + "{\"x\":" + js_number(points[i].x) + ",\"y\":" + js_number(points[i].y) + "}";
    return base64(json + "]");
}

}  // namespace mermaid

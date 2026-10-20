#include <mermaid/edge_geometry.h>

#include <mermaid/macros.h>

#include <charconv>
#include <cmath>

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
        case Shape::DoubleCircle: {
            const double r = MERMAID_MIN(w, h), len = std::hypot(dx, dy);
            return {node.center.x + r * dx / len, node.center.y + r * dy / len};
        }
        default: {  // dagre's intersect-rect
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

// A d3 curveBasis path as pieces: the first point, then lines and cubic curves.
struct basis_piece {
    bool cubic = false;
    Point c1, c2, to;
};

std::vector<basis_piece> basis_pieces(const std::vector<Point>& p) {
    std::vector<basis_piece> out;
    if (p.size() < 2) return out;
    if (p.size() == 2) return {{false, {}, {}, p[1]}};
    auto bezier = [&](const Point& a, const Point& b, const Point& c) {
        out.push_back({true,
                       {(2 * a.x + b.x) / 3, (2 * a.y + b.y) / 3},
                       {(a.x + 2 * b.x) / 3, (a.y + 2 * b.y) / 3},
                       {(a.x + 4 * b.x + c.x) / 6, (a.y + 4 * b.y + c.y) / 6}});
    };
    out.push_back({false, {}, {}, {(5 * p[0].x + p[1].x) / 6, (5 * p[0].y + p[1].y) / 6}});
    for (size_t i = 2; i < p.size(); ++i) bezier(p[i - 2], p[i - 1], p[i]);
    const Point& a = p[p.size() - 2];
    const Point& b = p.back();
    bezier(a, b, b);
    out.push_back({false, {}, {}, b});
    return out;
}

double round3(double v) { return std::round(v * 1000.0) / 1000.0; }

std::string xy(const Point& p) { return SvgWriter::num(round3(p.x)) + "," + SvgWriter::num(round3(p.y)); }

}  // namespace

std::string edge_geometry::basis_path(const std::vector<Point>& points) {
    if (points.empty()) return {};
    std::string d = "M" + xy(points.front());
    for (const basis_piece& piece : basis_pieces(points))
        d += piece.cubic ? "C" + xy(piece.c1) + "," + xy(piece.c2) + "," + xy(piece.to) : "L" + xy(piece.to);
    return d;
}

double edge_geometry::basis_length(const std::vector<Point>& points) {
    if (points.size() < 2) return 0;
    double total = 0;
    Point at = points.front();
    for (const basis_piece& piece : basis_pieces(points)) {
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

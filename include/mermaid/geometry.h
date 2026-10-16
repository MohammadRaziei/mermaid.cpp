// geometry.h - plain value types shared by every stage of the pipeline.
#pragma once

#include <algorithm>
#include <cmath>

namespace mermaid {

struct Point {
    double x = 0;
    double y = 0;

    friend bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
    friend Point operator+(const Point& a, const Point& b) { return {a.x + b.x, a.y + b.y}; }
    friend Point operator-(const Point& a, const Point& b) { return {a.x - b.x, a.y - b.y}; }
};

inline double distance(const Point& a, const Point& b) { return std::hypot(a.x - b.x, a.y - b.y); }

struct Size {
    double width = 0;
    double height = 0;
};

struct Rect {
    double x = 0;
    double y = 0;
    double width = 0;
    double height = 0;

    static Rect from_center(const Point& c, const Size& s) {
        return {c.x - s.width / 2, c.y - s.height / 2, s.width, s.height};
    }

    double right() const { return x + width; }
    double bottom() const { return y + height; }
    Point center() const { return {x + width / 2, y + height / 2}; }
    Size size() const { return {width, height}; }

    bool contains(const Point& p) const {
        return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom();
    }

    Rect united(const Rect& o) const {
        const double l = std::min(x, o.x), t = std::min(y, o.y);
        const double r = std::max(right(), o.right()), b = std::max(bottom(), o.bottom());
        return {l, t, r - l, b - t};
    }

    Rect expanded(double m) const { return {x - m, y - m, width + 2 * m, height + 2 * m}; }
};

}  // namespace mermaid

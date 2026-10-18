#include "shape_renderer.h"

#include <algorithm>
#include <functional>
#include <vector>
#include <mermaid/macros.h>

namespace mermaid::detail {

namespace {

using flowchart::Shape;

SvgWriter::Attrs with_style(SvgWriter::Attrs attrs, const std::string& style) {
    attrs.emplace_back("class", "basic label-container");
    if (!style.empty()) attrs.emplace_back("style", style);
    return attrs;
}

std::string n(double v) { return SvgWriter::num(v); }

class BoxShape final : public ShapeRenderer {
public:
    enum class Corner { Square, Rounded, Pill };
    explicit BoxShape(Corner corner) : corner_(corner) {}

    void draw(SvgWriter& out, const Rect& b, const std::string& style) const override {
        double radius = 0;
        if (corner_ == Corner::Rounded) radius = 5;
        if (corner_ == Corner::Pill) radius = b.height / 2;
        SvgWriter::Attrs attrs{{"x", n(b.x)}, {"y", n(b.y)}, {"width", n(b.width)}, {"height", n(b.height)}};
        if (radius > 0) {
            attrs.emplace_back("rx", n(radius));
            attrs.emplace_back("ry", n(radius));
        }
        out.leaf("rect", with_style(std::move(attrs), style));
    }

private:
    Corner corner_;
};

class SubroutineShape final : public ShapeRenderer {
public:
    void draw(SvgWriter& out, const Rect& b, const std::string& style) const override {
        BoxShape(BoxShape::Corner::Square).draw(out, b, style);
        const double inset = 8;
        out.leaf("path", with_style({{"d", "M" + n(b.x + inset) + " " + n(b.y) + " V" + n(b.bottom()) + " M" +
                                              n(b.right() - inset) + " " + n(b.y) + " V" + n(b.bottom())},
                                     {"fill", "none"}},
                                    "fill:none;" + style));
    }
};

class CylinderShape final : public ShapeRenderer {
public:
    void draw(SvgWriter& out, const Rect& b, const std::string& style) const override {
        const double rx = b.width / 2;
        const double ry = rx / (2.5 + b.width / 50);
        const double body = MERMAID_MAX(0.0, b.height - 2 * ry);
        const std::string arc = "a" + n(rx) + " " + n(ry) + " 0 0 0 ";
        out.leaf("path", with_style({{"d", "M" + n(b.x) + " " + n(b.y + ry) + " " + arc + n(b.width) + " 0 " + arc +
                                              n(-b.width) + " 0 l0 " + n(body) + " " + arc + n(b.width) + " 0 l0 " +
                                              n(-body)}},
                                    style));
    }
};

class CircleShape final : public ShapeRenderer {
public:
    explicit CircleShape(bool doubled) : doubled_(doubled) {}

    void draw(SvgWriter& out, const Rect& b, const std::string& style) const override {
        const double r = MERMAID_MAX(b.width, b.height) / 2;
        const Point c = b.center();
        out.leaf("circle", with_style({{"cx", n(c.x)}, {"cy", n(c.y)}, {"r", n(r)}}, style));
        if (doubled_)
            out.leaf("circle", with_style({{"cx", n(c.x)}, {"cy", n(c.y)}, {"r", n(MERMAID_MAX(0.0, r - 5))}}, style));
    }

private:
    bool doubled_;
};

class PolygonShape final : public ShapeRenderer {
public:
    using Corners = std::function<std::vector<Point>(const Rect&)>;
    explicit PolygonShape(Corners corners) : corners_(std::move(corners)) {}

    void draw(SvgWriter& out, const Rect& b, const std::string& style) const override {
        std::string points;
        for (const Point& p : corners_(b)) points += (points.empty() ? "" : " ") + n(p.x) + "," + n(p.y);
        out.leaf("polygon", with_style({{"points", points}}, style));
    }

private:
    Corners corners_;
};

std::vector<Point> diamond(const Rect& b) {
    const Point c = b.center();
    return {{c.x, b.y}, {b.right(), c.y}, {c.x, b.bottom()}, {b.x, c.y}};
}
std::vector<Point> hexagon(const Rect& b) {
    const double i = b.height / 4;
    const double cy = b.center().y;
    return {{b.x + i, b.y}, {b.right() - i, b.y}, {b.right(), cy}, {b.right() - i, b.bottom()}, {b.x + i, b.bottom()}, {b.x, cy}};
}
std::vector<Point> odd(const Rect& b) {
    return {{b.x, b.y}, {b.right(), b.y}, {b.right(), b.bottom()}, {b.x, b.bottom()}, {b.x + b.height / 4, b.center().y}};
}
std::vector<Point> lean_right(const Rect& b) {
    const double i = b.height / 4;
    return {{b.x + i, b.y}, {b.right(), b.y}, {b.right() - i, b.bottom()}, {b.x, b.bottom()}};
}
std::vector<Point> lean_left(const Rect& b) {
    const double i = b.height / 4;
    return {{b.x, b.y}, {b.right() - i, b.y}, {b.right(), b.bottom()}, {b.x + i, b.bottom()}};
}
std::vector<Point> trapezoid(const Rect& b) {
    const double i = b.height / 4;
    return {{b.x + i, b.y}, {b.right() - i, b.y}, {b.right(), b.bottom()}, {b.x, b.bottom()}};
}
std::vector<Point> inv_trapezoid(const Rect& b) {
    const double i = b.height / 4;
    return {{b.x, b.y}, {b.right(), b.y}, {b.right() - i, b.bottom()}, {b.x + i, b.bottom()}};
}

}  // namespace

const ShapeRenderer& ShapeRenderer::for_shape(Shape shape) {
    static const BoxShape square(BoxShape::Corner::Square);
    static const BoxShape rounded(BoxShape::Corner::Rounded);
    static const BoxShape pill(BoxShape::Corner::Pill);
    static const SubroutineShape subroutine;
    static const CylinderShape cylinder;
    static const CircleShape circle(false);
    static const CircleShape double_circle(true);
    static const PolygonShape diamond_shape(diamond);
    static const PolygonShape hexagon_shape(hexagon);
    static const PolygonShape odd_shape(odd);
    static const PolygonShape lean_right_shape(lean_right);
    static const PolygonShape lean_left_shape(lean_left);
    static const PolygonShape trapezoid_shape(trapezoid);
    static const PolygonShape inv_trapezoid_shape(inv_trapezoid);

    switch (shape) {
        case Shape::Default:
        case Shape::Square: return square;
        case Shape::Round: return rounded;
        case Shape::Stadium: return pill;
        case Shape::Subroutine: return subroutine;
        case Shape::Cylinder: return cylinder;
        case Shape::Circle: return circle;
        case Shape::DoubleCircle: return double_circle;
        case Shape::Diamond: return diamond_shape;
        case Shape::Hexagon: return hexagon_shape;
        case Shape::Odd: return odd_shape;
        case Shape::LeanRight: return lean_right_shape;
        case Shape::LeanLeft: return lean_left_shape;
        case Shape::Trapezoid: return trapezoid_shape;
        case Shape::InvTrapezoid: return inv_trapezoid_shape;
    }
    return square;
}

}  // namespace mermaid::detail

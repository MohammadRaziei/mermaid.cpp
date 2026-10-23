#include "node_renderer.h"

#include <cmath>

#include "rough_path.h"

#include "shape_geometry.h"
#include "style_css.h"

namespace mermaid::detail {

namespace {

using flowchart::Shape;

std::string translate(double x, double y) {
    return "translate(" + SvgWriter::num(x) + ", " + SvgWriter::num(y) + ")";
}

// flowchart.padding of Mermaid 12.2.
constexpr double kNodePadding = 15;
constexpr double kRoundRadius = 12;  // themeVariables.radius of redux-color
constexpr double kDoubleCircleGap = 12;  // doubleCircle.ts (neo)
constexpr double M_PI_RAD = 3.14159265358979323846;

std::string js(double v) { return js_number(v); }

}  // namespace

// shapes/cylinder.ts moves its label down by padding / 1.5; the other ported shapes keep it centred.
double node_renderer::label_shift(Shape shape) {
    return shape == Shape::Cylinder ? kNodePadding / 1.5 : 0;
}

void node_renderer::write(SvgWriter& out, const flowchart::Node& node, const flowchart::NodeLayout& layout,
                         const flowchart::Style& style) const {
    std::string classes = "node default";
    for (const std::string& cls : node.classes) classes += " " + cls;

    out.open("g", {{"class", classes},
                   {"id", svg_id_ + "-flowchart-" + node.id + "-" + std::to_string(node.dom_index)},
                   {"data-look", "neo"},
                   {"transform", translate(layout.center.x, layout.center.y)}});
    write_outline(out, layout.shape, layout.size, style);
    Point shift = label_offset(layout.shape, layout.size);
    shift.y += label_shift(layout.shape);
    write_label(out, node.label, layout.label, style, shift, node.markdown_label,
                !ignores_min_label_width(layout.shape) || layout.label_wrapped, layout.label_wrapped);
    out.close();
}

// shapes/stadium.ts: a polygon of the two straight sides and two 50-point arcs, drawn by rough.js.
void node_renderer::write_stadium(SvgWriter& out, const Size& size, const flowchart::Style& style,
                                  const std::string& inline_style) const {
    const double w = size.width, h = size.height, radius = h / 2;
    constexpr int kArcPoints = 50;
    const auto arc = [&](double cx, double cy, double start_deg, double end_deg, std::vector<Point>& into) {
        const double start = start_deg * M_PI_RAD / 180, step = (end_deg * M_PI_RAD / 180 - start) / (kArcPoints - 1);
        for (int i = 0; i < kArcPoints; ++i) {
            const double angle = start + i * step;
            into.push_back({-(cx + radius * std::cos(angle)), -(cy + radius * std::sin(angle))});
        }
    };
    std::vector<Point> points{{-w / 2 + radius, -h / 2}, {w / 2 - radius, -h / 2}};
    arc(-w / 2 + radius, 0, 90, 270, points);
    points.push_back({w / 2 - radius, h / 2});
    arc(w / 2 - radius, 0, 270, 450, points);
    write_rough_polygon(out, points, "", style, inline_style);
}

// A closed polygon drawn by rough.js (`rc.path`): a fill path and a stroke path in a group.
// userNodeOverrides: the style map wins over the theme.
void node_renderer::write_rough_polygon(SvgWriter& out, const std::vector<Point>& points, const std::string& transform,
                                        const flowchart::Style& style, const std::string& inline_style) const {
    const auto lookup = [&](const char* key, const std::string& fallback) {
        std::string value = fallback;
        for (const auto& [k, v] : style)
            if (k == key) value = v;
        return value;
    };
    std::string stroke_width = lookup("stroke-width", "1.3");
    if (stroke_width.size() > 2 && stroke_width.compare(stroke_width.size() - 2, 2, "px") == 0) stroke_width.resize(stroke_width.size() - 2);
    std::string dash = "0 0";
    if (const std::string given = lookup("stroke-dasharray", ""); !given.empty()) {
        std::vector<std::string> parts;
        std::string current;
        for (char c : given + " ") {
            if (c == ' ' || c == ',') { if (!current.empty()) parts.push_back(current); current.clear(); }
            else current += c;
        }
        if (parts.size() == 1) dash = parts[0] + " " + parts[0];
        else if (!parts.empty()) { dash.clear(); for (const auto& p : parts) dash += (dash.empty() ? "" : " ") + p; }
    }

    const rough_drawing drawing = rough_polygon(points, seed_);
    SvgWriter::Attrs group{{"class", "basic label-container outer-path"}};
    if (!transform.empty()) group.emplace_back("transform", transform);
    out.open("g", group);
    out.leaf("path", {{"d", drawing.fill_d}, {"stroke", "none"}, {"stroke-width", "0"}, {"fill", lookup("fill", theme_.main_bkg)}, {"style", inline_style}});
    out.leaf("path", {{"d", drawing.stroke_d},
                      {"stroke", lookup("stroke", theme_.node_border)},
                      {"stroke-width", stroke_width},
                      {"fill", "none"},
                      {"stroke-dasharray", dash},
                      {"style", inline_style}});
    out.close();
}

void node_renderer::write_outline(SvgWriter& out, Shape shape, const Size& size, const flowchart::Style& style) const {
    const std::string inline_style = important_shape_style(style);
    const Rect box{-size.width / 2, -size.height / 2, size.width, size.height};

    if (shape == Shape::Default || shape == Shape::Square) {
        out.leaf("rect", {{"class", "basic label-container"},
                          {"style", inline_style},
                          {"x", SvgWriter::num(box.x)},
                          {"y", SvgWriter::num(box.y)},
                          {"width", SvgWriter::num(box.width)},
                          {"height", SvgWriter::num(box.height)}});
    } else if (shape == Shape::Diamond) {
        // shapes/question.ts: a polygon of side s drawn from (0,0) and moved into place.
        const double s = size.width;
        SvgWriter::Attrs attrs{{"points", SvgWriter::num(s / 2) + ",0 " + SvgWriter::num(s) + "," + SvgWriter::num(-s / 2) + " " +
                                              SvgWriter::num(s / 2) + "," + SvgWriter::num(-s) + " 0," + SvgWriter::num(-s / 2)},
                               {"class", "label-container"},
                               {"transform", translate(-s / 2 + 0.5, s / 2)}};
        if (!inline_style.empty()) attrs.emplace_back("style", inline_style);
        out.leaf("polygon", attrs);
    } else if (shape == Shape::Cylinder) {
        // shapes/cylinder.ts: drawn from 0,0 and moved by translate(-w / 2, -(h / 2 + ry)).
        const double w = size.width;
        const double rx = w / 2;
        const double ry = rx / (2.5 + w / 50);
        const double h = size.height - 2 * ry;
        const auto num = [](double v) { return SvgWriter::num(v); };
        const std::string arc = "a" + num(rx) + "," + num(ry) + " 0,0,0 ";
        const std::string d = "M0," + num(ry) + " " + arc + num(w) + ",0 " + arc + num(-w) + ",0 l0," + num(h) + " " + arc +
                              num(w) + ",0 l0," + num(-h);
        out.leaf("path", {{"d", d},
                          {"class", "basic label-container outer-path"},
                          {"style", inline_style},
                          {"transform", translate(-w / 2, -(h / 2 + ry))}});
    } else if (shape == Shape::Stadium) {
        write_stadium(out, size, style, inline_style);
    } else if (shape == Shape::Round) {
        // roundedRect.ts: drawRect with rx = ry = themeVariables.radius
        out.leaf("rect", {{"class", "basic label-container"},
                          {"style", inline_style},
                          {"rx", js(kRoundRadius)},
                          {"ry", js(kRoundRadius)},
                          {"x", js(box.x)},
                          {"y", js(box.y)},
                          {"width", js(box.width)},
                          {"height", js(box.height)}});
    } else if (shape == Shape::Circle) {
        out.leaf("circle", {{"class", "basic label-container"},
                            {"style", inline_style},
                            {"r", js(size.width / 2)},
                            {"cx", "0"},
                            {"cy", "0"}});
    } else if (shape == Shape::DoubleCircle) {
        // doubleCircle.ts: an outer and an inner ring, 12 apart
        const double outer = size.width / 2;
        out.open("g", {{"class", "basic label-container"}, {"style", inline_style}});
        out.leaf("circle", {{"class", "outer-circle"}, {"style", inline_style}, {"r", js(outer)}, {"cx", "0"}, {"cy", "0"}});
        out.leaf("circle", {{"class", "inner-circle"}, {"style", inline_style}, {"r", js(outer - kDoubleCircleGap)}, {"cx", "0"}, {"cy", "0"}});
        out.close();
    } else if (const auto polygon = polygon_outline_of(shape, size)) {
        if (polygon->rough) {
            write_rough_polygon(out, polygon->points, "translate(" + js(polygon->shift.x) + "," + js(polygon->shift.y) + ")", style, inline_style);
        } else {
            // insertPolygonShape: drawn from the origin and moved by translate(-w / 2, h / 2)
            std::string points;
            for (const Point& p : polygon->points) points += (points.empty() ? "" : " ") + js(p.x) + "," + js(p.y);
            SvgWriter::Attrs attrs{{"points", points},
                                   {"class", "label-container"},
                                   {"transform", "translate(" + js(-polygon->anchor.width / 2) + "," + js(polygon->anchor.height / 2) + ")"}};
            if (!inline_style.empty()) attrs.emplace_back("style", inline_style);
            out.leaf("polygon", attrs);
        }
    }
}

void node_renderer::write_label(SvgWriter& out, const std::string& text, const Size& label, const flowchart::Style& style,
                               const Point& shift, bool markdown, bool table, bool wrapped) const {
    // addHtmlSpan (createText.ts): a table-cell div; a label held to the minimum width is a `table` of that width.
    const std::string color = important_label_style(style);
    out.open("g", {{"class", "label"},
                   {"style", color},
                   {"transform", translate(-label.width / 2 + shift.x, -label.height / 2 + shift.y)}});
    out.leaf("rect");
    out.open("foreignObject", {{"width", SvgWriter::num(label.width)}, {"height", SvgWriter::num(label.height)}});
    out.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                     {"style", computed_label_color(style) +
                                   (table ? std::string("display: table; white-space: ") + (wrapped ? "break-spaces" : "nowrap") +
                                              "; line-height: 1.5; max-width: 120px; text-align: center; width: 120px;"
                                              : "display: table-cell; white-space: nowrap; line-height: 1.5; max-width: 120px; "
                                                "text-align: center;")}});
    SvgWriter::Attrs span{{"class", markdown ? "nodeLabel markdown-node-label" : "nodeLabel"}};
    if (!color.empty()) span.emplace_back("style", color);
    out.open("span", span);
    out.open("p");
    size_t start = 0;
    while (true) {
        const size_t end = text.find('\n', start);
        out.text(text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        out.raw("<br />");
        start = end + 1;
    }
    out.close();  // p
    out.close();  // span
    out.close();  // div
    out.close();  // foreignObject
    out.close();  // label group
}

}  // namespace mermaid::detail

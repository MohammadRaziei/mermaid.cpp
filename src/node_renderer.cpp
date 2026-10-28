#include "node_renderer.h"

#include <cmath>

#include "rough_path.h"

#include "shape_geometry.h"
#include "markdown_label.h"
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
    write_outline(out, layout.shape, layout.size, layout.label, style);
    Point shift = label_offset(layout.shape, layout.size, layout.label);
    shift.y += label_shift(layout.shape);
    // triangle.ts puts the label near the base, flippedTriangle.ts near the top (padding / 2 = 7.5 from the edge)
    if (layout.shape == Shape::Triangle) shift.y += layout.size.height / 2 - layout.label.height / 2 - kNodePadding / 2;
    if (layout.shape == Shape::FlippedTriangle) shift.y += -layout.size.height / 2 + layout.label.height / 2 + kNodePadding / 2;
    if (layout.shape == Shape::Hourglass) {  // the label is empty: a 0 x 0 box
        write_empty_label(out, style, node.markdown_label);
    } else if (layout.shape == Shape::Fork || layout.shape == Shape::SmallCircle || layout.shape == Shape::LightningBolt ||
               layout.shape == Shape::FilledCircle || layout.shape == Shape::FramedCircle || layout.shape == Shape::CrossedCircle) {
        // these shapes make no label at all
    } else {
        write_label(out, node.label, layout.label, style, shift, node.markdown_label,
                    !ignores_min_label_width(layout.shape) || layout.label_wrapped, layout.label_wrapped,
                    writes_tight_label_transform(layout.shape));  // these shapes write their own transform: no space
    }
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

// forkJoin.ts: rc.rectangle filled solid with the line colour. The fill is the plain outline of the rectangle.
void node_renderer::write_fork(SvgWriter& out, const Rect& box, const flowchart::Style& style, const std::string& inline_style) const {
    const std::vector<Point> corners{{box.x, box.y}, {box.x + box.width, box.y}, {box.x + box.width, box.y + box.height}, {box.x, box.y + box.height}};
    const auto lookup = [&](const char* key, const std::string& fallback) {
        std::string value = fallback;
        for (const auto& [k, v] : style)
            if (k == key) value = v;
        return value;
    };
    std::string fill_d;
    for (size_t i = 0; i < corners.size(); ++i) fill_d += (i ? " L" : "M") + js(corners[i].x) + " " + js(corners[i].y);
    const rough_drawing drawing = rough_polygon(corners, seed_);
    out.open("g");
    out.leaf("path", {{"d", fill_d}, {"stroke", "none"}, {"stroke-width", "0"}, {"fill", lookup("fill", theme_.line_color)}, {"style", inline_style}});
    out.leaf("path", {{"d", drawing.stroke_d},
                      {"stroke", lookup("stroke", theme_.line_color)},
                      {"stroke-width", "1.3"},
                      {"fill", "none"},
                      {"stroke-dasharray", "0 0"},
                      {"style", inline_style}});
    out.close();
}

// The two <path>s of one rough.js drawing: the solid fill and the outline.
// userNodeOverrides: the style map wins over the theme.
void node_renderer::write_rough_paths(SvgWriter& out, const rough_drawing& drawing, const flowchart::Style& style,
                                      const std::string& inline_style, bool polygon_fill, bool stroke_only) const {
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

    SvgWriter::Attrs fill{{"d", drawing.fill_d}, {"stroke", "none"}, {"stroke-width", "0"}, {"fill", lookup("fill", theme_.main_bkg)}};
    if (polygon_fill) fill.emplace_back("fill-rule", "evenodd");
    fill.emplace_back("style", inline_style);
    if (!stroke_only) out.leaf("path", fill);
    out.leaf("path", {{"d", drawing.stroke_d},
                      {"stroke", lookup("stroke", theme_.node_border)},
                      {"stroke-width", stroke_width},
                      {"fill", "none"},
                      {"stroke-dasharray", dash},
                      {"style", inline_style}});
}

// A closed polygon drawn by rough.js (`rc.path`): a fill path and a stroke path in a group.
void node_renderer::write_rough_polygon(SvgWriter& out, const std::vector<Point>& points, const std::string& transform,
                                        const flowchart::Style& style, const std::string& inline_style,
                                        const char* group_class) const {
    const rough_drawing drawing = rough_polygon(points, seed_);
    SvgWriter::Attrs group{{"class", group_class}};
    if (!transform.empty()) group.emplace_back("transform", transform);
    out.open("g", group);
    write_rough_paths(out, drawing, style, inline_style, false);
    out.close();
}

// The fill <path> and the outline <path> of one rough.js drawing with explicit colours.
static void write_pair(SvgWriter& out, const rough_drawing& drawing, const std::string& fill, const std::string& stroke,
                       const std::string& stroke_width, const std::string& inline_style) {
    out.leaf("path", {{"d", drawing.fill_d}, {"stroke", "none"}, {"stroke-width", "0"}, {"fill", fill}, {"style", inline_style}});
    out.leaf("path", {{"d", drawing.stroke_d}, {"stroke", stroke}, {"stroke-width", stroke_width}, {"fill", "none"},
                      {"stroke-dasharray", "0 0"}, {"style", inline_style}});
}

// filledCircle.ts, stateEnd.ts, crossedCircle.ts: rc.circle drawings (roughness 0), label-less.
void node_renderer::write_circle_shape(SvgWriter& out, Shape shape, const flowchart::Style&, const std::string& inline_style) const {
    if (shape == Shape::FilledCircle) {
        out.open("g");
        write_pair(out, rough_circle(14), theme_.main_bkg, theme_.node_border, "1.3",
                   inline_style.empty() ? "fill: " + theme_.node_border + " !important;" : inline_style);
        out.close();
    } else if (shape == Shape::FramedCircle) {
        SvgWriter::Attrs group{{"class", "outer-path"}};
        if (theme_.node_shadow) group.emplace_back("style", "filter:url(#" + svg_id_ + "-drop-shadow-small)");
        out.open("g", group);
        write_pair(out, rough_circle(14), theme_.main_bkg, theme_.line_color, "2", inline_style);
        out.open("g");
        write_pair(out, rough_circle(14.0 * 5 / 14), theme_.node_border, theme_.node_border, "2", inline_style);
        out.close();
        out.close();
    } else {
        constexpr double radius = 30;
        const double q = radius * 2 / 2 * std::cos(3.14159265358979323846 / 4);  // createLine: half the line length times cos(45 degrees)
        const double q_y = radius * 2 / 2 * std::sin(3.14159265358979323846 / 4);
        out.open("g", {{"class", "outer-path"}});
        write_pair(out, rough_circle(radius * 2), theme_.main_bkg, theme_.node_border, "1.3", inline_style);
        out.open("g");
        const rough_drawing cross = rough_open_subpaths({{{-q, q_y}, {q, -q_y}}, {{q, q_y}, {-q, -q_y}}}, seed_);
        write_pair(out, cross, theme_.main_bkg, theme_.node_border, "1.3", inline_style);
        out.close();
        out.close();
    }
}

// mergePaths (shapes.ts): the fill and the outline of one drawing joined into a single path.
void node_renderer::write_merged_path(SvgWriter& out, const rough_drawing& drawing, const flowchart::Style& style,
                                      const std::string& inline_style) const {
    const auto lookup = [&](const char* key, const std::string& fallback) {
        std::string value = fallback;
        for (const auto& [k, v] : style)
            if (k == key) value = v;
        return value;
    };
    std::string stroke_width = lookup("stroke-width", "1.3");
    if (stroke_width.size() > 2 && stroke_width.compare(stroke_width.size() - 2, 2, "px") == 0) stroke_width.resize(stroke_width.size() - 2);
    out.leaf("path", {{"d", drawing.fill_d + " " + drawing.stroke_d},
                      {"fill", lookup("fill", theme_.main_bkg)},
                      {"fill-opacity", "1"},
                      {"stroke", lookup("stroke", theme_.node_border)},
                      {"stroke-width", stroke_width},
                      {"stroke-opacity", "1"},
                      {"style", inline_style}});
}

// Document shapes: several drawings, one of them possibly in a <g> inside the group that carries class and transform.
void node_renderer::write_rough_layers(SvgWriter& out, const polygon_outline& outline, const flowchart::Style& style,
                                       const std::string& inline_style) const {
    SvgWriter::Attrs group{{"class", outline.group_class}};
    if (outline.has_shift) group.emplace_back("transform", "translate(" + js(outline.shift.x) + outline.separator + js(outline.shift.y) + ")");
    if (outline.group_style) group.emplace_back("style", "");
    out.open("g", group);
    for (const polygon_outline::layer& layer : outline.layers) {
        if (layer.nested) {
            if (layer.hidden) out.open("g", {{"stroke-opacity", "0"}});
            else out.open("g");
        }
        if (!layer.more.empty()) {
            std::vector<std::vector<Point>> runs{layer.points};
            runs.insert(runs.end(), layer.more.begin(), layer.more.end());
            write_rough_paths(out, rough_open_subpaths(runs, seed_), style, inline_style, false);
            if (layer.nested) out.close();
            continue;
        }
        if (layer.stroke_only) {
            write_rough_paths(out, rough_stroke_path(layer.points, !layer.open, seed_), style, inline_style, false, true);
            if (layer.nested) out.close();
            continue;
        }
        if (layer.merged) {
            write_merged_path(out, rough_polygon(layer.points, seed_), style, inline_style);
            if (layer.nested) out.close();
            continue;
        }
        write_rough_paths(out, layer.polygon_fill ? rough_polygon_filled_plainly(layer.points, seed_) : rough_polygon(layer.points, seed_),
                          style, inline_style, layer.polygon_fill);
        if (layer.nested) out.close();
    }
    out.close();
}

void node_renderer::write_outline(SvgWriter& out, Shape shape, const Size& size, const Size& label, const flowchart::Style& style) const {
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
    } else if (const auto arcs = arc_shape_of(shape, label)) {
        // cloud.ts, bang.ts: one path, laid out in its nominal box and moved by translate(-w / 2, -h / 2)
        out.leaf("path", {{"class", "basic label-container"},
                          {"style", inline_style},
                          {"d", arcs->d},
                          {"transform", translate(-arcs->nominal.width / 2, -arcs->nominal.height / 2)}});
    } else if (shape == Shape::LinedCylinder) {
        // linedCylinder.ts: like the cylinder plus a second lid line, moved by translate(-w / 2, -(h / 2 + ry)).
        const double w = size.width, rx = w / 2, ry = rx / (2.5 + w / 50);
        const double h = size.height - 2 * ry;
        const std::string arc = "a" + js(rx) + "," + js(ry) + " 0,0,0 ";
        const std::string d = "M0," + js(ry) + " " + arc + js(w) + ",0 " + arc + js(-w) + ",0 l0," + js(h) + " " + arc + js(w) + ",0 l0," +
                              js(-h) + " M0," + js(ry + h * 0.1) + " " + arc + js(w) + ",0";
        out.leaf("path", {{"d", d},
                          {"class", "basic label-container outer-path"},
                          {"style", inline_style},
                          {"transform", translate(-w / 2, -(h / 2 + ry))}});
    } else if (shape == Shape::TiltedCylinder) {
        // tiltedCylinder.ts: drawn from 0,0 and moved by translate(-w / 2, h / 2 ) (sic, the space is Mermaid's).
        const double h = label.height + 12, ry = h / 2, rx = ry / (2.5 + h / 50), w = label.width + rx + 12;
        const std::string arc = "a" + js(rx) + "," + js(ry) + " 0,0,";
        const std::string nl = "\n    ";  // createCylinderPathD3 is a template literal: its line breaks stay in the attribute
        const std::string d = "M0,0" + nl + arc + "1 0," + js(-h) + nl + "l" + js(w) + ",0" + nl + arc + "1 0," + js(h) + nl + "M" + js(w) + "," +
                              js(-h) + nl + arc + "0 0," + js(h) + nl + "l" + js(-w) + ",0";
        out.leaf("path", {{"d", d},
                          {"class", "basic label-container outer-path"},
                          {"style", inline_style},
                          {"transform", "translate(" + js(-w / 2) + ", " + js(h / 2) + " )"}});
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
    } else if (shape == Shape::Text) {
        // text.ts: a rectangle with the class `text`, no border of its own
        out.leaf("rect", {{"class", "text"},
                          {"style", inline_style},
                          {"rx", "0"},
                          {"ry", "0"},
                          {"x", js(box.x)},
                          {"y", js(box.y)},
                          {"width", js(box.width)},
                          {"height", js(box.height)}});
    } else if (shape == Shape::FilledCircle || shape == Shape::FramedCircle || shape == Shape::CrossedCircle) {
        write_circle_shape(out, shape, style, inline_style);
    } else if (shape == Shape::SmallCircle) {
        // stateStart.ts: a plain circle with the small drop shadow of the neo look
        SvgWriter::Attrs attrs{{"class", "state-start"}, {"r", js(size.width / 2)}, {"width", js(size.width)}, {"height", js(size.height)}};
        if (theme_.node_shadow) attrs.emplace_back("style", "filter:url(#" + svg_id_ + "-drop-shadow-small)");
        out.leaf("circle", attrs);
    } else if (shape == Shape::Fork) {
        write_fork(out, box, style, inline_style);
    } else if (shape == Shape::DataStore) {  // datastore.ts: drawRect, then stroke-dasharray = "width height" leaves the sides unstroked
        out.leaf("rect", {{"class", "basic label-container"}, {"style", inline_style}, {"x", SvgWriter::num(box.x)}, {"y", SvgWriter::num(box.y)},
                          {"width", SvgWriter::num(box.width)}, {"height", SvgWriter::num(box.height)},
                          {"stroke-dasharray", SvgWriter::num(box.width) + " " + SvgWriter::num(box.height)}});
    } else if (shape == Shape::Person) {  // person.ts: the body rect, then the head circle
        const person_geometry g = person_geometry_of(label);
        const auto num = [](double v) { return SvgWriter::num(v); };
        out.open("g", {{"class", "basic label-container"}});
        out.leaf("rect", {{"x", num(-g.w / 2)}, {"y", num(g.body_top)}, {"width", num(g.w)}, {"height", num(g.body_height)},
                          {"rx", num(g.body_radius)}, {"ry", num(g.body_radius)}, {"style", inline_style}});
        out.leaf("circle", {{"cx", "0"}, {"cy", num(g.top + g.head_radius)}, {"r", num(g.head_radius)}, {"style", inline_style}});
        out.close();
    } else if (shape == Shape::Console || shape == Shape::Browser) {
        // consoleWindow.ts / browser.ts: a rounded rect and the decoration, drawn in the accent colour
        const std::string accent = theme_.node_border.empty() ? theme_.line_color : theme_.node_border;
        const double w = size.width, top = box.y, bar = shape == Shape::Console ? 20.0 : 18.0;
        const auto num = [](double v) { return SvgWriter::num(v); };
        out.open("g", {{"class", "basic label-container"}});
        out.leaf("rect", {{"x", num(-w / 2)}, {"y", num(top)}, {"width", num(w)}, {"height", num(size.height)}, {"rx", "12"}, {"ry", "12"}, {"style", inline_style}});
        if (shape == Shape::Console) {
            out.open("text", {{"x", num(-w / 2 + 12)}, {"y", num(top + 16)}, {"class", "console-glyph"},
                              {"style", "font-family:monospace;font-weight:bold;font-size:14px;fill:" + accent}});
            out.text(">_");
            out.close();
        } else {
            out.leaf("line", {{"x1", num(-w / 2)}, {"y1", num(top + bar)}, {"x2", num(w / 2)}, {"y2", num(top + bar)}, {"style", "stroke:" + accent + ";stroke-width:1px"}});
            for (int i = 0; i < 3; ++i)
                out.leaf("circle", {{"cx", num(-w / 2 + 12 + i * 9)}, {"cy", num(top + bar / 2)}, {"r", "2.5"}, {"style", "fill:" + accent + ";stroke:none"}});
            out.leaf("rect", {{"class", "browser-address-bar"}, {"x", num(-w / 2 + 44)}, {"y", num(top + 4)}, {"width", num(MERMAID_MAX(w - 56, 10.0))},
                              {"height", num(bar - 8)}, {"rx", "3"}, {"ry", "3"}, {"style", "fill:none;stroke:" + accent + ";stroke-width:1px;opacity:0.6"}});
        }
        out.close();
    } else if (shape == Shape::Bucket) {
        // bucket.ts: a group with the cup (two arcs) and the rim ellipse, drawn round the node's origin
        const bucket_geometry g = bucket_geometry_of(size);
        const std::string d = "M" + js(-g.w / 2) + "," + js(g.top_y) + " L" + js(-g.bottom_w / 2) + "," + js(g.bottom_y) + " A" + js(g.bottom_w / 2) + "," +
                              js(g.rim_ry) + " 0 0 0 " + js(g.bottom_w / 2) + "," + js(g.bottom_y) + " L" + js(g.w / 2) + "," + js(g.top_y) + " A" +
                              js(g.w / 2) + "," + js(g.rim_ry) + " 0 0 0 " + js(-g.w / 2) + "," + js(g.top_y) + " Z";
        const std::string accent = theme_.node_border.empty() ? theme_.line_color : theme_.node_border;
        out.open("g", {{"class", "basic label-container"}});
        out.leaf("path", {{"d", d}, {"style", inline_style}});
        out.leaf("ellipse", {{"cx", "0"}, {"cy", js(g.top_y)}, {"rx", js(g.w / 2)}, {"ry", js(g.rim_ry)},
                             {"style", "fill:none;stroke:" + accent + ";stroke-width:1px"}});
        out.close();
    } else if (const auto polygon = polygon_outline_of(shape, size, label)) {
        if (polygon->plain_path) {
            std::string d;
            for (const Point& p : polygon->points) d += (d.empty() ? "M" : " L") + js(p.x) + "," + js(p.y);
            out.leaf("path", {{"d", d + " Z"}, {"class", "basic label-container"}, {"style", inline_style}});
        } else if (!polygon->layers.empty()) {
            write_rough_layers(out, *polygon, style, inline_style);
        } else if (polygon->rough) {
            write_rough_polygon(out, polygon->points, polygon->has_shift ? "translate(" + js(polygon->shift.x) + polygon->separator + js(polygon->shift.y) + ")" : std::string(), style, inline_style,
                                  polygon->group_class);
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

void node_renderer::write_empty_label(SvgWriter& out, const flowchart::Style& style, bool markdown) const {
    out.open("g", {{"class", "label"}, {"style", important_label_style(style)}, {"transform", "translate(0, 0)"}});
    out.leaf("rect");
    out.open("foreignObject", {{"width", "0"}, {"height", "0"}});
    out.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                     {"style", computed_label_color(style) + "display: table-cell; white-space: nowrap; line-height: 1.5; max-width: 120px; text-align: center;"}});
    out.leaf("span", {{"class", markdown ? "nodeLabel markdown-node-label" : "nodeLabel"}});
    out.close();  // div
    out.close();  // foreignObject
    out.close();  // label group
}

void node_renderer::write_label(SvgWriter& out, const std::string& text, const Size& label, const flowchart::Style& style,
                               const Point& shift, bool markdown, bool table, bool wrapped, bool tight) const {
    // addHtmlSpan (createText.ts): a table-cell div; a label held to the minimum width is a `table` of that width.
    const std::string color = important_label_style(style);
    out.open("g", {{"class", "label"},
                   {"style", color},
                   {"transform", tight ? "translate(" + js(-label.width / 2 + shift.x) + "," + js(-label.height / 2 + shift.y) + ")"
                                       : translate(-label.width / 2 + shift.x, -label.height / 2 + shift.y)}});
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
    if (markdown) {
        out.raw(markdown_label(text).html());  // <p> with <strong> / <em>
    } else {
        out.paragraph(text);
    }
    out.close();  // span
    out.close();  // div
    out.close();  // foreignObject
    out.close();  // label group
}

}  // namespace mermaid::detail

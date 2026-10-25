#include "edge_renderer.h"

#include <algorithm>

#include <mermaid/edge_geometry.h>

#include <cctype>
#include <cmath>

#include "markdown_label.h"
#include "style_css.h"

namespace mermaid::detail {

namespace {

using flowchart::Head;
using flowchart::Stroke;

const char* head_name(Head head) {
    switch (head) {
        case Head::Arrow: return "point";
        case Head::Cross: return "cross";
        case Head::Circle: return "circle";
        case Head::None: break;
    }
    return "";
}

}  // namespace

namespace {

// One <marker> of Mermaid 12.2 (rendering-util/rendering-elements/markers.js). The `-margin`
// variants are the larger ones used by the neo look. Fill and stroke come from the stylesheet
// (class `arrowMarkerPath`).
struct marker_spec {
    const char* name;      // after "<id>_flowchart-v2-"
    const char* classes;
    const char* view_box;
    const char* ref_x;
    const char* ref_y;
    const char* width;
    const char* height;
    const char* shape;     // path | polygon | circle
    const char* geometry;  // d, points, or empty for circle
    const char* style;
};

constexpr const char* kThin = "stroke-width: 1; stroke-dasharray: 1, 0;";
constexpr const char* kNone = "stroke-width: 0; stroke-dasharray: 1, 0;";
constexpr const char* kCross = "stroke-width: 2; stroke-dasharray: 1, 0;";

const marker_spec kMarkers[] = {
    {"pointEnd", "marker flowchart-v2", "0 0 10 10", "5", "5", "8", "8", "path", "M 0 0 L 10 5 L 0 10 z", kThin},
    {"pointStart", "marker flowchart-v2", "0 0 10 10", "4.5", "5", "8", "8", "path", "M 0 5 L 10 10 L 10 0 z", kThin},
    {"pointEnd-margin", "marker flowchart-v2", "0 0 11.5 14", "11.5", "7", "10.5", "14", "path", "M 0 0 L 11.5 7 L 0 14 z", kNone},
    {"pointStart-margin", "marker flowchart-v2", "0 0 11.5 14", "1", "7", "11.5", "14", "polygon", "0,7 11.5,14 11.5,0", kNone},
    {"circleEnd", "marker flowchart-v2", "0 0 10 10", "11", "5", "11", "11", "circle", "", kThin},
    {"circleStart", "marker flowchart-v2", "0 0 10 10", "-1", "5", "11", "11", "circle", "", kThin},
    {"circleEnd-margin", "marker flowchart-v2", "0 0 10 10", "12.25", "5", "14", "14", "circle", "", kNone},
    {"circleStart-margin", "marker flowchart-v2", "0 0 10 10", "-2", "5", "14", "14", "circle", "", kNone},
    {"crossEnd", "marker cross flowchart-v2", "0 0 11 11", "12", "5.2", "11", "11", "path", "M 1,1 l 9,9 M 10,1 l -9,9", kCross},
    {"crossStart", "marker cross flowchart-v2", "0 0 11 11", "-1", "5.2", "11", "11", "path", "M 1,1 l 9,9 M 10,1 l -9,9", kCross},
    {"crossEnd-margin", "marker cross flowchart-v2", "0 0 15 15", "17.7", "7.5", "12", "12", "path", "M 1,1 L 14,14 M 1,14 L 14,1", "stroke-width: 2.5;"},
    {"crossStart-margin", "marker cross flowchart-v2", "0 0 15 15", "-3.5", "7.5", "12", "12", "path", "M 1,1 L 14,14 M 1,14 L 14,1", "stroke-width: 2.5; stroke-dasharray: 1, 0;"},
};

const marker_spec* find_marker(const std::string& name) {
    for (const marker_spec& m : kMarkers)
        if (name == m.name) return &m;
    return nullptr;
}

// markerOffsets (lineWithOffset.ts): how far the path is pulled back from the node (arrow_point only).

// markerOffsets2 (edges.js): the gap kept free at each end of the dash pattern.
double dash_offset(Head head) {
    switch (head) {
        case Head::Arrow: return 4;
        case Head::Cross:
        case Head::Circle: return 12.5;
        case Head::None: break;
    }
    return 0;
}

// generateDashArray (edges.js): `0 start (2 2)... end` with as many pairs as fit.
std::string dash_array(double length, double start, double end, bool patterned) {
    if (!patterned)
        return "0 " + SvgWriter::num(start) + " " + SvgWriter::num(length - start - end) + " " + SvgWriter::num(end);
    const double pairs = std::floor((length - start - end) / 4);
    std::string middle;
    for (int i = 0; i < pairs; ++i) middle += (i ? " 2 2" : "2 2");
    return "0 " + SvgWriter::num(start) + " " + middle + " " + SvgWriter::num(end);
}

// The `style` text edges.js builds from the edge styles (and its odd doubling).
std::string path_style_text(const flowchart::Edge& edge) {
    std::string joined;
    for (const auto& [key, value] : edge.style) joined += (joined.empty() ? "" : ";") + key + ":" + value;
    if (joined.empty()) return ";";
    return joined + ";;;" + joined;
}

// edgeMarker.ts: the colour is the first `stroke:` of that text.
std::string stroke_color(const std::string& style_text) {
    size_t at = 0;
    while ((at = style_text.find("stroke:", at)) != std::string::npos) {
        if (at == 0 || style_text[at - 1] == ';' || style_text[at - 1] == ' ') {
            const size_t end = style_text.find(';', at);
            return style_text.substr(at + 7, end == std::string::npos ? end : end - at - 7);
        }
        at += 7;
    }
    return {};
}

std::string color_id(const std::string& color) {
    std::string out;
    for (char c : color) out += std::isalnum(static_cast<unsigned char>(c)) ? c : '_';
    return out;
}

std::string marker_name(Head head, bool start) {
    return std::string(head_name(head)) + (start ? "Start" : "End") + "-margin";
}

}  // namespace

void EdgeRenderer::write_marker(SvgWriter& out, const std::string& name, const std::string& id, const std::string& color,
                                bool filled) const {
    const marker_spec& m = *find_marker(name);
    out.open("marker", {{"id", id},
                        {"class", m.classes},
                        {"viewBox", m.view_box},
                        {"refX", m.ref_x},
                        {"refY", m.ref_y},
                        {"markerUnits", "userSpaceOnUse"},
                        {"markerWidth", m.width},
                        {"markerHeight", m.height},
                        {"orient", "auto"}});
    SvgWriter::Attrs attrs;
    const std::string shape = m.shape;
    if (shape == "circle") attrs = {{"cx", "5"}, {"cy", "5"}, {"r", "5"}};
    else if (shape == "polygon") attrs = {{"points", m.geometry}};
    else attrs = {{"d", m.geometry}};
    attrs.emplace_back("class", "arrowMarkerPath");
    attrs.emplace_back("style", m.style);
    if (!color.empty()) {
        attrs.emplace_back("stroke", color);
        if (filled) attrs.emplace_back("fill", color);
    }
    out.leaf(shape == "circle" ? "circle" : shape == "polygon" ? "polygon" : "path", attrs);
    out.close();
}

void EdgeRenderer::write_markers(SvgWriter& out) const {
    for (const marker_spec& m : kMarkers) write_marker(out, m.name, svg_id_ + "_flowchart-v2-" + m.name, "", false);
}

void EdgeRenderer::write_colored_markers(SvgWriter& out) const {
    for (const colored_marker& c : colored_) write_marker(out, c.source, c.id, c.color, c.filled);
}

std::string EdgeRenderer::marker_url(Head head, bool start, const std::string& style_text) {
    const std::string name = marker_name(head, start);
    std::string id = svg_id_ + "_flowchart-v2-" + name;
    const std::string color = stroke_color(style_text);
    if (!color.empty()) {
        id += "_" + color_id(color);
        bool known = false;
        for (const colored_marker& c : colored_) known = known || c.id == id;
        if (!known) colored_.push_back({id, name, color, head == Head::Arrow});
    }
    return "url(#" + id + ")";
}

bool EdgeRenderer::is_drawn(const flowchart::EdgeLayout& layout) {
    return layout.points.size() >= 2;  // invisible edges are drawn too, with the `invisible` thickness
}

void EdgeRenderer::write_path(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout, const Ends& ends) {
    if (!is_drawn(layout)) return;

    // Route as Mermaid draws it: clipped to the outlines, pulled back from the arrow tips, rounded.
    // (the edge's curve, `basis` unless set, after fixCorners)
    std::vector<Point> route = layout.points;
    // (the loop of a cluster already sits on the border of the cluster box: it is not clipped again)
    if (ends.tail && ends.head && !(layout.self_loop && ends.tail_is_cluster)) route = edge_geometry::clip_to_nodes(route, *ends.tail, *ends.head);
    const std::string recorded_points = edge_geometry::points_attribute(route);  // data-points: before the cluster cuts
    if (ends.head_is_cluster) route = edge_geometry::cut_path_at_intersect(layout.points, ends.head->rect());
    if (ends.tail_is_cluster) {
        std::reverse(route.begin(), route.end());
        route = edge_geometry::cut_path_at_intersect(route, ends.tail->rect());
        std::reverse(route.begin(), route.end());
    }
    const std::vector<Point> drawn =
        edge_geometry::shorten_ends(edge_geometry::fix_corners(route), edge_geometry::path_offset(edge.start), edge_geometry::path_offset(edge.end));
    const double length = edge_geometry::curve_length(edge.curve, drawn);

    // thickness picks the first class pair; the pattern is `solid` unless the stroke is dotted
    // (flowDb passes the stroke as the pattern, which only `dotted` and `dashed` recognise).
    const std::string thickness = edge.stroke == Stroke::Thick ? "thick" : "normal";
    const bool dotted = edge.stroke == Stroke::Dotted;
    // flowDb gives an invisible edge no `flowchart-link` classes, so only the stroke classes remain
    const std::string classes = edge.stroke == Stroke::Invisible
                                    ? std::string("edge-thickness-invisible edge-pattern-solid")
                                    : "edge-thickness-" + thickness + " edge-pattern-" + (dotted ? "dotted" : "solid") +
                                          " edge-thickness-normal edge-pattern-solid flowchart-link";

    const std::string style_text = path_style_text(edge);
    const std::string style = "stroke-dasharray: " + dash_array(length, dash_offset(edge.start), dash_offset(edge.end), dotted) +
                              "; stroke-dashoffset: 0;" + style_text;

    const std::string d = edge_geometry::curve_path(edge.curve, drawn);
    // edges.js resolveEdgeLabelPosition: unless the middle point of the clipped route lies on the drawn path, the
    // label follows the path by the distance its middle moved from the middle of dagre's route.
    if (layout.has_label && !edge.label.empty() && !route.empty() &&
        !edge_geometry::is_label_coordinate_in_path(route[route.size() / 2], d)) {
        const Point now = edge_geometry::label_position(route), before = edge_geometry::label_position(layout.points);
        label_shifts_[edge.id] = {now.x - before.x, now.y - before.y};
    }

    SvgWriter::Attrs attrs{{"d", d},
                           {"id", svg_id_ + "-" + edge.id},
                           {"class", classes},
                           {"style", style},
                           {"data-edge", "true"},
                           {"data-et", "edge"},
                           {"data-id", edge.id},
                           {"data-points", recorded_points},
                           {"data-look", "neo"}};
    if (edge.start != Head::None) attrs.emplace_back("marker-start", marker_url(edge.start, true, style_text));
    if (edge.end != Head::None) attrs.emplace_back("marker-end", marker_url(edge.end, false, style_text));
    out.leaf("path", attrs);
}

void EdgeRenderer::write_label(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) const {
    if (!is_drawn(layout)) return;

    const auto translate = [](double x, double y) { return "translate(" + SvgWriter::num(x) + ", " + SvgWriter::num(y) + ")"; };
    const bool labelled = layout.has_label && !edge.label.empty();
    Rect box = labelled ? layout.label : Rect{0, 0, 0, 0};
    if (const auto shift = label_shifts_.find(edge.id); labelled && shift != label_shifts_.end()) {
        box.x += shift->second.x;
        box.y += shift->second.y;
    }

    // edges.js insertEdgeLabel: a group at the label centre, the HTML label centred inside it.
    // An edge without a label gets the same markup, empty, with no position.
    SvgWriter::Attrs group{{"class", "edgeLabel"}};
    if (labelled) group.emplace_back("transform", translate(box.center().x, box.center().y));
    out.open("g", group);
    out.open("g", {{"class", "label"}, {"data-id", edge.id}, {"transform", translate(-box.width / 2, -box.height / 2)}});
    out.open("foreignObject", {{"width", SvgWriter::num(box.width)}, {"height", SvgWriter::num(box.height)}});
    out.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                     {"class", "labelBkg"},
                     {"style", computed_label_color(edge.style) +
                                   (labelled && layout.label_wrapped
                                        ? "display: table; white-space: break-spaces; line-height: 1.5; max-width: 200px; text-align: center; width: 200px;"
                                        : "display: table-cell; white-space: nowrap; line-height: 1.5; max-width: 200px; text-align: center;")}});
    // a `color` of linkStyle colours the label text: on the div as computed, on the span as written
    SvgWriter::Attrs span_attrs;
    if (const std::string color = important_label_style(edge.style); !color.empty()) span_attrs.emplace_back("style", color);
    span_attrs.emplace_back("class", "edgeLabel");
    out.open("span", span_attrs);
    if (labelled && edge.markdown_label) {
        out.raw(markdown_label(edge.label).html());
    } else if (labelled) {
        out.open("p");
        size_t start = 0;
        while (true) {
            const size_t end = edge.label.find('\n', start);
            out.text(edge.label.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos) break;
            out.raw("<br />");
            start = end + 1;
        }
        out.close();  // p
    }
    out.close();  // span
    out.close();  // div
    out.close();  // foreignObject
    out.close();  // label group
    out.close();  // edgeLabel group
}

}  // namespace mermaid::detail

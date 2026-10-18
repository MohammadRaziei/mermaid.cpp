#include "edge_renderer.h"

#include "style_css.h"
#include "text_block.h"

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

std::string EdgeRenderer::marker_id(Head head, bool start) const {
    return svg_id_ + "_flowchart-v2-" + head_name(head) + (start ? "Start" : "End");
}

namespace {

// One <marker> of Mermaid 12.2 (rendering-util/rendering-elements/markers.js). The `-margin`
// variants are the larger ones used by the neo look. Fill and stroke come from the stylesheet
// (class `arrowMarkerPath`).
struct MarkerSpec {
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

const MarkerSpec kMarkers[] = {
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

}  // namespace

void EdgeRenderer::write_markers(SvgWriter& out) const {
    for (const MarkerSpec& m : kMarkers) {
        out.open("marker", {{"id", svg_id_ + "_flowchart-v2-" + m.name},
                            {"class", m.classes},
                            {"viewBox", m.view_box},
                            {"refX", m.ref_x},
                            {"refY", m.ref_y},
                            {"markerUnits", "userSpaceOnUse"},
                            {"markerWidth", m.width},
                            {"markerHeight", m.height},
                            {"orient", "auto"}});
        const std::string shape = m.shape;
        if (shape == "circle")
            out.leaf("circle", {{"cx", "5"}, {"cy", "5"}, {"r", "5"}, {"class", "arrowMarkerPath"}, {"style", m.style}});
        else if (shape == "polygon")
            out.leaf("polygon", {{"points", m.geometry}, {"class", "arrowMarkerPath"}, {"style", m.style}});
        else
            out.leaf("path", {{"d", m.geometry}, {"class", "arrowMarkerPath"}, {"style", m.style}});
        out.close();
    }
}

bool EdgeRenderer::is_drawn(const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) {
    return edge.stroke != Stroke::Invisible && layout.points.size() >= 2;
}

void EdgeRenderer::write_path(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) const {
    if (!is_drawn(edge, layout)) return;

    std::string d;
    for (size_t i = 0; i < layout.points.size(); ++i)
        d += (i == 0 ? "M" : " L") + SvgWriter::num(layout.points[i].x) + " " + SvgWriter::num(layout.points[i].y);

    std::string style;
    if (edge.stroke == Stroke::Thick) style += "stroke-width:3.5px;";
    if (edge.stroke == Stroke::Dotted) style += "stroke-dasharray:2;";
    style += shape_declarations(edge.style);

    SvgWriter::Attrs attrs{{"id", edge.id}, {"class", "flowchart-link"}, {"d", d}, {"fill", "none"}};
    if (!style.empty()) attrs.emplace_back("style", style);
    if (edge.start != Head::None) attrs.emplace_back("marker-start", "url(#" + marker_id(edge.start, true) + ")");
    if (edge.end != Head::None) attrs.emplace_back("marker-end", "url(#" + marker_id(edge.end, false) + ")");
    out.leaf("path", attrs);
}

void EdgeRenderer::write_label(SvgWriter& out, const flowchart::Edge& edge, const flowchart::EdgeLayout& layout) const {
    if (!is_drawn(edge, layout) || !layout.has_label || edge.label.empty()) return;

    const Rect box = layout.label.expanded(2);
    out.open("g", {{"class", "edgeLabel"}});
    out.leaf("rect", {{"class", "labelBkg"},
                      {"x", SvgWriter::num(box.x)},
                      {"y", SvgWriter::num(box.y)},
                      {"width", SvgWriter::num(box.width)},
                      {"height", SvgWriter::num(box.height)},
                      {"fill", theme_.edge_label_bg}});
    write_text_block(out, layout.label.center(), edge.label, line_height_);
    out.close();
}

}  // namespace mermaid::detail

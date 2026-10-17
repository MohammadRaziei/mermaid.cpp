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

void EdgeRenderer::write_markers(SvgWriter& out) const {
    for (const Head head : {Head::Arrow, Head::Cross, Head::Circle}) {
        for (const bool start : {false, true}) {
            const std::string ref = head == Head::Arrow ? "9" : head == Head::Circle ? "6" : "5";
            out.open("marker", {{"id", marker_id(head, start)},
                                {"viewBox", "0 0 10 10"},
                                {"refX", ref},
                                {"refY", "5"},
                                {"markerUnits", "userSpaceOnUse"},
                                {"markerWidth", "8"},
                                {"markerHeight", "8"},
                                {"orient", start ? "auto-start-reverse" : "auto"}});
            if (head == Head::Arrow)
                out.leaf("path", {{"d", "M 0 0 L 10 5 L 0 10 z"}, {"fill", theme_.line}, {"stroke", theme_.line}});
            else if (head == Head::Cross)
                out.leaf("path", {{"d", "M 1,1 L 9,9 M 9,1 L 1,9"}, {"fill", "none"}, {"stroke", theme_.line}, {"stroke-width", "2"}});
            else
                out.leaf("circle", {{"cx", "5"}, {"cy", "5"}, {"r", "4"}, {"fill", theme_.line}, {"stroke", theme_.line}});
            out.close();
        }
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

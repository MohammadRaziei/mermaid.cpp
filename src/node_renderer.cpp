#include "node_renderer.h"

#include "shape_renderer.h"
#include "style_css.h"

namespace mermaid::detail {

namespace {

using flowchart::Shape;

std::string translate(double x, double y) {
    return "translate(" + SvgWriter::num(x) + ", " + SvgWriter::num(y) + ")";
}

// flowchart.padding of Mermaid 12.2.
constexpr double kNodePadding = 15;

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
    write_label(out, node.label, layout.label, style, label_shift(layout.shape));
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
    } else {
        // Not ported yet: the previous outline, in local coordinates.
        ShapeRenderer::for_shape(shape).draw(out, box, shape_declarations(style));
    }
}

void node_renderer::write_label(SvgWriter& out, const std::string& text, const Size& label, const flowchart::Style& style,
                               double dy) const {
    // addHtmlSpan (createText.ts): a table-cell div that is widened to the minimum label width.
    const std::string color = important_label_style(style);
    out.open("g", {{"class", "label"},
                   {"style", color},
                   {"transform", translate(-label.width / 2, -label.height / 2 + dy)}});
    out.leaf("rect");
    out.open("foreignObject", {{"width", SvgWriter::num(label.width)}, {"height", SvgWriter::num(label.height)}});
    out.open("div", {{"xmlns", "http://www.w3.org/1999/xhtml"},
                     {"style", computed_label_color(style) +
                                   "display: table; white-space: nowrap; line-height: 1.5; max-width: 120px; "
                                   "text-align: center; width: 120px;"}});
    SvgWriter::Attrs span{{"class", "nodeLabel"}};
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

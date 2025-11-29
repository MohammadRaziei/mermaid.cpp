#include "renderer.hpp"

#include <iomanip>

namespace mermaid {

void SvgVisitor::emit_header(double width, double height) {
    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
       << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    ss << "  <defs>\n";
    ss << "    <marker id=\"arrowhead\" markerWidth=\"10\" markerHeight=\"7\" refX=\"10\" refY=\"3.5\" orient=\"auto\">\n";
    ss << "      <polygon points=\"0 0, 10 3.5, 0 7\" fill=\"#000\" />\n";
    ss << "    </marker>\n";
    ss << "  </defs>\n";
}

void SvgVisitor::emit_footer() {
    ss << "</svg>\n";
}

void SvgVisitor::visit(SequenceDiagramNode &node) {
    current_sequence = &node;
    emit_header(node.width, node.height);

    for (auto &participant : node.participants) {
        participant->accept(*this);
    }

    for (auto &message : node.messages) {
        message->accept(*this);
    }

    emit_footer();
    result_svg = ss.str();
    current_sequence = nullptr;
}

void SvgVisitor::visit(ParticipantNode &node) {
    const double rect_width = 120.0;
    const double rect_height = 40.0;

    double rect_x = node.x - rect_width / 2.0;
    double rect_y = node.y - rect_height / 2.0;

    ss << "  <g class=\"participant\">\n";
    ss << "    <rect x=\"" << rect_x << "\" y=\"" << rect_y << "\" width=\"" << rect_width
       << "\" height=\"" << rect_height << "\" fill=\"#e0e0e0\" stroke=\"#000\" />\n";
    ss << "    <text x=\"" << node.x << "\" y=\"" << node.y + 6
       << "\" font-family=\"sans-serif\" font-size=\"14\" text-anchor=\"middle\">"
       << node.label << "</text>\n";
    ss << "    <line x1=\"" << node.x << "\" y1=\"" << node.y + rect_height / 2.0
       << "\" x2=\"" << node.x << "\" y2=\"" << node.lifeline_end_y
       << "\" stroke=\"#000\" stroke-dasharray=\"4 4\" />\n";
    ss << "  </g>\n";
}

void SvgVisitor::visit(MessageNode &node) {
    double text_x = (node.from_x + node.to_x) / 2.0;
    double text_y = node.y - 6.0;

    ss << "  <g class=\"message\">\n";
    ss << "    <line x1=\"" << node.from_x << "\" y1=\"" << node.y << "\" x2=\"" << node.to_x
       << "\" y2=\"" << node.y << "\" stroke=\"#000\" marker-end=\"url(#arrowhead)\" />\n";
    ss << "    <text x=\"" << text_x << "\" y=\"" << text_y
       << "\" font-family=\"sans-serif\" font-size=\"13\" text-anchor=\"middle\">"
       << node.text << "</text>\n";
    ss << "  </g>\n";
}

} // namespace mermaid


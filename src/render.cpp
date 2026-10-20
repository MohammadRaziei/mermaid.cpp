#include <mermaid/render.h>

#include <array>
#include <cctype>

#include <mermaid/browser_text_measurer.h>
#include <mermaid/error.h>
#include <mermaid/flowchart_layout.h>
#include <mermaid/flowchart_parser.h>

#include "preprocess.h"

namespace mermaid {

namespace {

std::string first_word(const std::string& text) {
    size_t end = 0;
    while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end])) && text[end] != ':') ++end;
    return text.substr(0, end);
}

bool is_flowchart(const std::string& word) {
    return word == "graph" || word == "flowchart" || word == "flowchart-elk";
}

// Diagram types Mermaid knows but this library does not draw (yet).
bool is_known_other(const std::string& word) {
    static const std::array<const char*, 20> known{
        "sequenceDiagram", "classDiagram", "classDiagram-v2", "stateDiagram", "stateDiagram-v2", "erDiagram",
        "gantt", "pie", "journey", "mindmap", "gitGraph", "timeline", "quadrantChart", "requirementDiagram",
        "sankey-beta", "xychart-beta", "block-beta", "packet-beta", "kanban", "architecture-beta"};
    for (const char* k : known)
        if (word == k) return true;
    return word.rfind("C4", 0) == 0;
}

}  // namespace

std::string render_svg(std::string_view source, const RenderOptions& options) {
    const std::vector<detail::Statement> statements = detail::split_statements(source);
    if (statements.empty()) throw ParseError("empty diagram", 1);

    const std::string word = first_word(statements.front().text);
    if (!is_flowchart(word)) {
        if (is_known_other(word)) throw UnsupportedDiagram(word + " diagrams are not supported yet");
        throw ParseError("unknown diagram type '" + word + "'", statements.front().line);
    }

    const flowchart::Model model = flowchart::Parser().parse(source);
    const browser_text_measurer measurer;
    const flowchart::Layout layout = flowchart::LayoutEngine(measurer).layout(model);
    return flowchart::render_svg(model, layout, options);
}

}  // namespace mermaid

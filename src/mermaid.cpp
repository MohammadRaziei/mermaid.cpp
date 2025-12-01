#include "mermaid.h"

#include "layout.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "renderer.hpp"
#include <stdexcept>

namespace mermaid {

std::string render_to_svg(const std::string &mermaid_source) {
    Lexer lexer{mermaid_source};
    Parser parser{lexer};

    auto ast = parser.parse();
    auto *seq = dynamic_cast<SequenceDiagramNode *>(ast.get());
    if (!seq) {
        throw std::runtime_error("Unsupported diagram type");
    }

    SequenceLayoutStrategy layout;
    layout.layout(*seq);

    SvgVisitor visitor;
    ast->accept(visitor);

    return visitor.result_svg;
}

} // namespace mermaid


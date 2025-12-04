#pragma once

#include "ast.hpp"
#include <sstream>
#include <string>

namespace mermaid {

class AstVisitor {
public:
    virtual ~AstVisitor() = default;
    virtual void visit(SequenceDiagramNode &node) = 0;
    virtual void visit(ParticipantNode &node) = 0;
    virtual void visit(MessageNode &node) = 0;
};

class SvgVisitor : public AstVisitor {
public:
    void visit(SequenceDiagramNode &node) override;
    void visit(ParticipantNode &node) override;
    void visit(MessageNode &node) override;

    std::string result_svg;

private:
    void emit_header(double width, double height);
    void emit_footer();

    SequenceDiagramNode *current_sequence{nullptr};
    std::ostringstream ss;
    int message_count{0};
};

} // namespace mermaid

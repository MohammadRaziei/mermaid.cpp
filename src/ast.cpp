#include "ast.hpp"
#include "renderer.hpp"

namespace mermaid {

void ParticipantNode::accept(AstVisitor &v) { v.visit(*this); }
void MessageNode::accept(AstVisitor &v) { v.visit(*this); }
void SequenceDiagramNode::accept(AstVisitor &v) { v.visit(*this); }
void NoteNode::accept(AstVisitor &v) { v.visit(*this); }

} // namespace mermaid

#pragma once

#include "ast.hpp"
#include "lexer.hpp"
#include <memory>

namespace mermaid {

class Parser {
public:
    explicit Parser(Lexer &lexer);

    std::unique_ptr<AstNode> parse();

private:
    std::unique_ptr<SequenceDiagramNode> parse_sequence_diagram();
    void parse_participant(SequenceDiagramNode &diagram);
    void parse_actor(SequenceDiagramNode &diagram);
    void parse_note(SequenceDiagramNode &diagram);
    void parse_message(SequenceDiagramNode &diagram);
    void parse_block(SequenceDiagramNode &diagram);
    void parse_activation(SequenceDiagramNode &diagram);

    void advance();
    bool match(TokenType type);
    Token consume(TokenType type, const std::string &message);
    void skip_newlines();

    Lexer &m_lexer;
    Token m_current{TokenType::Eof, "", 0, 0};
    bool m_has_current{false};
};

} // namespace mermaid

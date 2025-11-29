#include "parser.hpp"

#include <stdexcept>

namespace mermaid {

Parser::Parser(Lexer &lexer) : m_lexer(lexer) {}

std::unique_ptr<AstNode> Parser::parse() {
    advance();
    return parse_sequence_diagram();
}

std::unique_ptr<SequenceDiagramNode> Parser::parse_sequence_diagram() {
    consume(TokenType::KeywordSequenceDiagram, "Expected 'sequenceDiagram'");
    skip_newlines();

    auto diagram = std::make_unique<SequenceDiagramNode>();

    while (m_current.type != TokenType::Eof) {
        if (m_current.type == TokenType::Newline) {
            advance();
            continue;
        }
        if (m_current.type == TokenType::KeywordParticipant) {
            parse_participant(*diagram);
            continue;
        }
        if (m_current.type == TokenType::Identifier) {
            parse_message(*diagram);
            continue;
        }
        throw std::runtime_error("Unexpected token in diagram body");
    }

    return diagram;
}

void Parser::parse_participant(SequenceDiagramNode &diagram) {
    consume(TokenType::KeywordParticipant, "Expected 'participant'");
    Token id = consume(TokenType::Identifier, "Expected participant identifier");

    std::string display = id.lexeme;
    if (m_current.type == TokenType::KeywordAs) {
        advance();
        Token alias = consume(TokenType::Identifier, "Expected alias identifier after 'as'");
        display = alias.lexeme;
    }

    diagram.participants.push_back(std::make_unique<ParticipantNode>(id.lexeme, display));

    if (m_current.type == TokenType::Newline) {
        advance();
    }
    skip_newlines();
}

void Parser::parse_message(SequenceDiagramNode &diagram) {
    Token from = consume(TokenType::Identifier, "Expected sender identifier");
    consume(TokenType::Arrow, "Expected message arrow");
    Token to = consume(TokenType::Identifier, "Expected receiver identifier");
    consume(TokenType::Colon, "Expected ':' after receiver");
    Token text = consume(TokenType::Text, "Expected message text");

    diagram.messages.push_back(std::make_unique<MessageNode>(from.lexeme, to.lexeme, text.lexeme));

    if (m_current.type == TokenType::Newline) {
        advance();
    }
    skip_newlines();
}

void Parser::advance() {
    if (!m_has_current || m_current.type != TokenType::Eof) {
        m_current = m_lexer.next();
        m_has_current = true;
    }
}

bool Parser::match(TokenType type) {
    if (m_current.type == type) {
        advance();
        return true;
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string &message) {
    if (m_current.type == type) {
        Token consumed = m_current;
        advance();
        return consumed;
    }
    throw std::runtime_error(message);
}

void Parser::skip_newlines() {
    while (m_current.type == TokenType::Newline) {
        advance();
    }
}

} // namespace mermaid


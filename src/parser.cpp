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
        if (m_current.type == TokenType::KeywordActor) {
            parse_actor(*diagram);
            continue;
        }
        if (m_current.type == TokenType::KeywordNote) {
            parse_note(*diagram);
            continue;
        }
        if (m_current.type == TokenType::KeywordLoop ||
            m_current.type == TokenType::KeywordAlt ||
            m_current.type == TokenType::KeywordOpt ||
            m_current.type == TokenType::KeywordPar ||
            m_current.type == TokenType::KeywordBreak ||
            m_current.type == TokenType::KeywordCritical ||
            m_current.type == TokenType::KeywordRect) {
            parse_block(*diagram);
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

    diagram.participants.push_back(std::make_unique<ParticipantNode>(id.lexeme, display, "participant"));

    if (m_current.type == TokenType::Newline) {
        advance();
    }
    skip_newlines();
}

void Parser::parse_actor(SequenceDiagramNode &diagram) {
    consume(TokenType::KeywordActor, "Expected 'actor'");
    Token id = consume(TokenType::Identifier, "Expected actor identifier");

    std::string display = id.lexeme;
    if (m_current.type == TokenType::KeywordAs) {
        advance();
        Token alias = consume(TokenType::Identifier, "Expected alias identifier after 'as'");
        display = alias.lexeme;
    }

    diagram.participants.push_back(std::make_unique<ParticipantNode>(id.lexeme, display, "actor"));

    if (m_current.type == TokenType::Newline) {
        advance();
    }
    skip_newlines();
}

void Parser::parse_note(SequenceDiagramNode &diagram) {
    consume(TokenType::KeywordNote, "Expected 'note'");
    
    // Parse placement: "right of", "left of", or "over"
    std::string placement = "right of"; // default
    if (m_current.type == TokenType::Identifier) {
        Token placement_token = consume(TokenType::Identifier, "Expected placement");
        placement = placement_token.lexeme;
        if (placement == "right" || placement == "left") {
            // Expect "of" after "right" or "left"
            consume(TokenType::Identifier, "Expected 'of' after placement");
            placement += " of";
        }
    }
    
    // Parse actor
    Token actor = consume(TokenType::Identifier, "Expected actor identifier for note");
    consume(TokenType::Colon, "Expected ':' after actor");
    
    // Parse note text
    Token text = consume(TokenType::Text, "Expected note text");
    
    diagram.notes.push_back(std::make_unique<NoteNode>(actor.lexeme, placement, text.lexeme));
    
    if (m_current.type == TokenType::Newline) {
        advance();
    }
    skip_newlines();
}

void Parser::parse_message(SequenceDiagramNode &diagram) {
    Token from = consume(TokenType::Identifier, "Expected sender identifier");
    
    // Parse arrow (could be any arrow type)
    Token arrow = m_current;
    if (m_current.type != TokenType::ArrowSolid &&
        m_current.type != TokenType::ArrowSolidCross &&
        m_current.type != TokenType::ArrowDotted &&
        m_current.type != TokenType::ArrowDottedCross) {
        throw std::runtime_error("Expected message arrow");
    }
    advance(); // consume arrow
    
    // Check for activation marker (+ or -)
    bool activate_target = false;
    bool deactivate_source = false;
    if (m_current.type == TokenType::Plus) {
        activate_target = true;
        advance();
    } else if (m_current.type == TokenType::Minus) {
        deactivate_source = true;
        advance();
    }
    
    Token to = consume(TokenType::Identifier, "Expected receiver identifier");
    consume(TokenType::Colon, "Expected ':' after receiver");
    Token text = consume(TokenType::Text, "Expected message text");

    auto message = std::make_unique<MessageNode>(from.lexeme, to.lexeme, text.lexeme);
    message->activate_target = activate_target;
    message->deactivate_source = deactivate_source;
    message->is_dotted = (arrow.type == TokenType::ArrowDotted || arrow.type == TokenType::ArrowDottedCross);
    message->has_cross = (arrow.type == TokenType::ArrowSolidCross || arrow.type == TokenType::ArrowDottedCross);
    
    diagram.messages.push_back(std::move(message));

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

void Parser::parse_block(SequenceDiagramNode &diagram) {
    // Determine block type
    std::string block_type;
    if (m_current.type == TokenType::KeywordLoop) block_type = "loop";
    else if (m_current.type == TokenType::KeywordAlt) block_type = "alt";
    else if (m_current.type == TokenType::KeywordOpt) block_type = "opt";
    else if (m_current.type == TokenType::KeywordPar) block_type = "par";
    else if (m_current.type == TokenType::KeywordBreak) block_type = "break";
    else if (m_current.type == TokenType::KeywordCritical) block_type = "critical";
    else if (m_current.type == TokenType::KeywordRect) block_type = "rect";
    else throw std::runtime_error("Unknown block type");
    
    advance(); // consume block keyword
    
    // Parse optional label (text after block keyword)
    std::string label;
    // Collect tokens until newline (identifiers, spaces are not tokens)
    // We'll just collect identifiers and assume spaces between them.
    // This is a simplification for loop_basic.
    while (m_current.type == TokenType::Identifier) {
        if (!label.empty()) label += " ";
        label += m_current.lexeme;
        advance();
        // If next token is also identifier, continue
    }
    // Skip newline
    if (m_current.type == TokenType::Newline) {
        advance();
    }
    
    // Create block node
    auto block = std::make_unique<BlockNode>(block_type, label);
    
    // Record start message index (number of messages already parsed)
    block->start_message_index = diagram.messages.size();
    
    // For now, just skip until "end"
    // TODO: Properly parse nested messages and blocks
    while (m_current.type != TokenType::KeywordEnd && m_current.type != TokenType::Eof) {
        // Skip newlines
        if (m_current.type == TokenType::Newline) {
            advance();
            continue;
        }
        // If we encounter another block start, recursively parse it
        if (m_current.type == TokenType::KeywordLoop ||
            m_current.type == TokenType::KeywordAlt ||
            m_current.type == TokenType::KeywordOpt ||
            m_current.type == TokenType::KeywordPar ||
            m_current.type == TokenType::KeywordBreak ||
            m_current.type == TokenType::KeywordCritical ||
            m_current.type == TokenType::KeywordRect) {
            parse_block(diagram);
            continue;
        }
        // If we encounter a message, parse it and add to diagram.messages
        if (m_current.type == TokenType::Identifier) {
            parse_message(diagram);
            continue;
        }
        // Skip other tokens (should not happen)
        advance();
    }
    
    if (m_current.type == TokenType::KeywordEnd) {
        advance(); // consume "end"
    }
    
    // Record end message index (exclusive)
    block->end_message_index = diagram.messages.size();
    
    // Add block to diagram
    diagram.blocks.push_back(std::move(block));
    
    // Skip newlines after block
    skip_newlines();
}

void Parser::skip_newlines() {
    while (m_current.type == TokenType::Newline) {
        advance();
    }
}

} // namespace mermaid

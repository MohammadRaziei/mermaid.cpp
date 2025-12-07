#include "lexer.hpp"

#include <cctype>
#include <stdexcept>

namespace mermaid {

Lexer::Lexer(const std::string &source) : m_source(source) {}

const Token &Lexer::peek() {
    if (!m_has_peek) {
        m_peeked = read_token();
        m_has_peek = true;
    }
    return m_peeked;
}

Token Lexer::next() {
    if (m_has_peek) {
        m_has_peek = false;
        return m_peeked;
    }
    return read_token();
}

Token Lexer::read_token() {
    if (m_expect_text) {
        return lex_text();
    }
    if (m_expect_block_label) {
        return lex_block_label();
    }

    skip_whitespace();

    if (m_pos >= m_source.size()) {
        return Token{TokenType::Eof, "", m_line, m_column};
    }

    char c = m_source[m_pos];

    if (c == '\n') {
        advance();
        m_line += 1;
        m_column = 1;
        return Token{TokenType::Newline, "\n", m_line - 1, 1};
    }

    if (c == ':') {
        advance();
        m_expect_text = true;
        return Token{TokenType::Colon, ":", m_line, m_column - 1};
    }

    if (c == '+') {
        advance();
        return Token{TokenType::Plus, "+", m_line, m_column - 1};
    }

    if (c == '-') {
        // Could be start of arrow or minus sign
        // Check for -> or --> (dotted arrow starts with --)
        if ((m_pos + 1) < m_source.size() && m_source[m_pos + 1] == '>') {
            return lex_arrow();
        }
        // Also check for --> where first char is - and second is -
        if ((m_pos + 2) < m_source.size() && m_source[m_pos + 1] == '-' && m_source[m_pos + 2] == '>') {
            return lex_arrow();
        }
        advance();
        return Token{TokenType::Minus, "-", m_line, m_column - 1};
    }

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return lex_identifier();
    }

    throw std::runtime_error("Unexpected character in input");
}

void Lexer::skip_whitespace() {
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
            continue;
        }
        // Handle line comments starting with %%
        if (c == '%' && (m_pos + 1) < m_source.size() && m_source[m_pos + 1] == '%') {
            // Skip until newline or end of input
            while (m_pos < m_source.size() && m_source[m_pos] != '\n') {
                advance();
            }
            continue;
        }
        break;
    }
}

Token Lexer::lex_identifier() {
    std::size_t start_pos = m_pos;
    std::size_t start_col = m_column;
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' ) {
            advance();
        } else {
            break;
        }
    }
    std::string lexeme = m_source.substr(start_pos, m_pos - start_pos);
    if (lexeme == "sequenceDiagram") {
        return Token{TokenType::KeywordSequenceDiagram, lexeme, m_line, start_col};
    }
    if (lexeme == "participant") {
        return Token{TokenType::KeywordParticipant, lexeme, m_line, start_col};
    }
    if (lexeme == "actor") {
        return Token{TokenType::KeywordActor, lexeme, m_line, start_col};
    }
    if (lexeme == "note") {
        return Token{TokenType::KeywordNote, lexeme, m_line, start_col};
    }
    if (lexeme == "as") {
        return Token{TokenType::KeywordAs, lexeme, m_line, start_col};
    }
    if (lexeme == "loop") {
        return Token{TokenType::KeywordLoop, lexeme, m_line, start_col};
    }
    if (lexeme == "alt") {
        return Token{TokenType::KeywordAlt, lexeme, m_line, start_col};
    }
    if (lexeme == "opt") {
        return Token{TokenType::KeywordOpt, lexeme, m_line, start_col};
    }
    if (lexeme == "par") {
        return Token{TokenType::KeywordPar, lexeme, m_line, start_col};
    }
    if (lexeme == "break") {
        return Token{TokenType::KeywordBreak, lexeme, m_line, start_col};
    }
    if (lexeme == "critical") {
        return Token{TokenType::KeywordCritical, lexeme, m_line, start_col};
    }
    if (lexeme == "rect") {
        return Token{TokenType::KeywordRect, lexeme, m_line, start_col};
    }
    if (lexeme == "end") {
        return Token{TokenType::KeywordEnd, lexeme, m_line, start_col};
    }
    if (lexeme == "else") {
        return Token{TokenType::KeywordElse, lexeme, m_line, start_col};
    }
    if (lexeme == "and") {
        return Token{TokenType::KeywordAnd, lexeme, m_line, start_col};
    }
    if (lexeme == "option") {
        return Token{TokenType::KeywordOption, lexeme, m_line, start_col};
    }
    if (lexeme == "activate") {
        return Token{TokenType::KeywordActivate, lexeme, m_line, start_col};
    }
    if (lexeme == "deactivate") {
        return Token{TokenType::KeywordDeactivate, lexeme, m_line, start_col};
    }
    return Token{TokenType::Identifier, lexeme, m_line, start_col};
}

Token Lexer::lex_arrow() {
    std::size_t start_col = m_column;
    
    // Check if it's a dotted arrow (-->) or solid arrow (->)
    bool is_dotted = false;
    advance(); // first '-'
    if (m_pos < m_source.size() && m_source[m_pos] == '-') {
        is_dotted = true;
        advance(); // second '-'
    }
    
    // Must have '>' after '-'
    if (m_pos >= m_source.size() || m_source[m_pos] != '>') {
        throw std::runtime_error("Expected '>' after '-' in arrow");
    }
    advance(); // '>'
    
    // Check for cross arrow (>>)
    bool has_cross = match('>');
    
    // Build lexeme
    std::string lexeme;
    if (is_dotted) {
        lexeme = "--";
    } else {
        lexeme = "-";
    }
    lexeme += ">";
    if (has_cross) {
        lexeme += ">";
    }
    
    // Return appropriate token
    if (is_dotted) {
        if (has_cross) {
            return Token{TokenType::ArrowDottedCross, lexeme, m_line, start_col};
        } else {
            return Token{TokenType::ArrowDotted, lexeme, m_line, start_col};
        }
    } else {
        if (has_cross) {
            return Token{TokenType::ArrowSolidCross, lexeme, m_line, start_col};
        } else {
            return Token{TokenType::ArrowSolid, lexeme, m_line, start_col};
        }
    }
}

Token Lexer::lex_text() {
    while (m_pos < m_source.size() && (m_source[m_pos] == ' ' || m_source[m_pos] == '\t')) {
        advance();
    }

    std::size_t start_col = m_column;
    std::size_t start_pos = m_pos;
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos];
        if (c == '\n') {
            break;
        }
        advance();
    }
    m_expect_text = false;
    std::string lexeme = m_source.substr(start_pos, m_pos - start_pos);
    while (!lexeme.empty() && (lexeme.back() == ' ' || lexeme.back() == '\t')) {
        lexeme.pop_back();
    }
    return Token{TokenType::Text, lexeme, m_line, start_col};
}

Token Lexer::lex_block_label() {
    // Skip whitespace after keyword (already skipped by skip_whitespace)
    while (m_pos < m_source.size() && (m_source[m_pos] == ' ' || m_source[m_pos] == '\t')) {
        advance();
    }
    std::size_t start_col = m_column;
    std::size_t start_pos = m_pos;
    while (m_pos < m_source.size()) {
        char c = m_source[m_pos];
        if (c == '\n') {
            break;
        }
        advance();
    }
    m_expect_block_label = false;
    std::string lexeme = m_source.substr(start_pos, m_pos - start_pos);
    // Trim trailing whitespace
    while (!lexeme.empty() && (lexeme.back() == ' ' || lexeme.back() == '\t')) {
        lexeme.pop_back();
    }
    return Token{TokenType::Text, lexeme, m_line, start_col};
}

void Lexer::advance() {
    m_pos += 1;
    m_column += 1;
}

bool Lexer::match(char expected) {
    if (m_pos >= m_source.size() || m_source[m_pos] != expected) {
        return false;
    }
    advance();
    return true;
}

} // namespace mermaid

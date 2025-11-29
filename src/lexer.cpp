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

    if (c == '-' && (m_pos + 1) < m_source.size() && m_source[m_pos + 1] == '>') {
        return lex_arrow();
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
    if (lexeme == "as") {
        return Token{TokenType::KeywordAs, lexeme, m_line, start_col};
    }
    return Token{TokenType::Identifier, lexeme, m_line, start_col};
}

Token Lexer::lex_arrow() {
    std::size_t start_col = m_column;
    advance(); // '-'
    advance(); // '>'
    if (match('>')) {
        return Token{TokenType::Arrow, "->>", m_line, start_col};
    }
    return Token{TokenType::Arrow, "->", m_line, start_col};
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


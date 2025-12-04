#pragma once

#include <cstddef>
#include <string>

namespace mermaid {

enum class TokenType {
    KeywordSequenceDiagram,
    KeywordParticipant,
    KeywordAs,
    Identifier,
    ArrowSolid,        // ->
    ArrowSolidCross,   // ->>
    ArrowDotted,       // -->
    ArrowDottedCross,  // -->>
    Plus,              // + (activation)
    Minus,             // - (deactivation)
    Colon,
    Text,
    Newline,
    Eof
};

struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

class Lexer {
public:
    explicit Lexer(const std::string &source);

    Token next();
    const Token &peek();

private:
    Token read_token();
    void skip_whitespace();
    Token lex_identifier();
    Token lex_arrow();
    Token lex_text();
    void advance();
    bool match(char expected);

    const std::string m_source;
    std::size_t m_pos{0};
    std::size_t m_line{1};
    std::size_t m_column{1};
    bool m_has_peek{false};
    Token m_peeked{};
    bool m_expect_text{false};
};

} // namespace mermaid

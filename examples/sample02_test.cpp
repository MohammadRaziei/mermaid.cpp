#include "includes/lexer.hpp"
#include <iostream>

int main() {
    std::string source = "Service-->>-Client";
    mermaid::Lexer lexer(source);
    
    std::cout << "Tokens from: " << source << std::endl;
    while (true) {
        auto token = lexer.next();
        std::cout << "Token type: ";
        switch (token.type) {
            case mermaid::TokenType::Identifier: std::cout << "Identifier"; break;
            case mermaid::TokenType::ArrowSolid: std::cout << "ArrowSolid"; break;
            case mermaid::TokenType::ArrowSolidCross: std::cout << "ArrowSolidCross"; break;
            case mermaid::TokenType::ArrowDotted: std::cout << "ArrowDotted"; break;
            case mermaid::TokenType::ArrowDottedCross: std::cout << "ArrowDottedCross"; break;
            case mermaid::TokenType::Plus: std::cout << "Plus"; break;
            case mermaid::TokenType::Minus: std::cout << "Minus"; break;
            case mermaid::TokenType::Eof: std::cout << "Eof"; break;
            default: std::cout << "Other"; break;
        }
        std::cout << " lexeme: '" << token.lexeme << "'" << std::endl;
        
        if (token.type == mermaid::TokenType::Eof) break;
    }
    
    return 0;
}

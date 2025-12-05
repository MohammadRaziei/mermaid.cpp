#include "lexer.hpp"
#include <iostream>
#include <fstream>
#include <string>

int main() {
    std::ifstream file("../tests/data/sequence_diagram/loop_basic.mermaid");
    if (!file) {
        std::cerr << "Failed to open file\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    mermaid::Lexer lexer(source);
    while (true) {
        mermaid::Token token = lexer.next();
        std::cout << "Token type: " << static_cast<int>(token.type) << " lexeme: '" << token.lexeme << "' line: " << token.line << " col: " << token.column << std::endl;
        if (token.type == mermaid::TokenType::Eof) break;
    }
    return 0;
}

// preprocess.h (private) - shared source normalisation for all diagram kinds.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mermaid::detail {

struct Statement {
    std::string text;  // trimmed, never empty
    int line = 0;      // 1-based line of the first character
};

// Drops YAML frontmatter, `%%{ ... }%%` directives and `%%` comments, then
// splits on newlines and on `;` outside quotes and brackets.
std::vector<Statement> split_statements(std::string_view source);

std::string trim(std::string_view s);

}  // namespace mermaid::detail

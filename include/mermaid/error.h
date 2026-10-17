// error.h - exceptions thrown by the public API.
#pragma once

#include <stdexcept>
#include <string>

namespace mermaid {

// Syntax error in the diagram source; `line` is 1-based (0 = unknown).
class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message, int line)
        : std::runtime_error(message), line_(line) {}
    int line() const noexcept { return line_; }

private:
    int line_;
};

// The diagram type is recognised (or not) but has no implementation yet.
class UnsupportedDiagram : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace mermaid

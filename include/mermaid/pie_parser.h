// pie_parser.h - text -> pie::Model.
#pragma once

#include <string_view>

#include <mermaid/pie.h>

namespace mermaid::pie {

class Parser {
public:
    // Parses a `pie` chart. Throws mermaid::ParseError.
    Model parse(std::string_view source) const;
};

}  // namespace mermaid::pie

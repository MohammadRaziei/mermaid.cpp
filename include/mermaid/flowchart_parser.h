// flowchart_parser.h - text -> flowchart::Model.
#pragma once

#include <string>
#include <string_view>

#include <mermaid/flowchart.h>

namespace mermaid::flowchart {

class Parser {
public:
    // Parses a `graph`/`flowchart` diagram. Throws mermaid::ParseError.
    Model parse(std::string_view source) const;
};

}  // namespace mermaid::flowchart

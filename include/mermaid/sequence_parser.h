// sequence_parser.h - text -> sequence::Model.
#pragma once

#include <string_view>

#include <mermaid/sequence.h>

namespace mermaid::sequence {

class Parser {
public:
    // Parses a `sequenceDiagram`. Throws mermaid::ParseError.
    Model parse(std::string_view source) const;
};

}  // namespace mermaid::sequence

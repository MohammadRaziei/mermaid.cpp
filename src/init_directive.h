// init_directive.h (private) - reads the `%%{init: {...}}%%` directives of a diagram source.
#pragma once

#include <string_view>

#include <mermaid/flowchart.h>
#include <mermaid/pie.h>
#include <mermaid/sequence.h>

namespace mermaid::detail {

// Merges every `init` / `initialize` directive, in source order. A directive that is not valid JSON is
// ignored (Mermaid logs and carries on); keys this library does not use are skipped.
flowchart::InitConfig parse_init_directives(std::string_view source);

// The `sequence` keys and the top-level `wrap` of the same directives.
sequence::Config parse_sequence_init(std::string_view source);

// The `pie` keys of the same directives; a theme other than `default`, `themeVariables` and `fontFamily` (which recolour or restyle the
// chart) are recorded as unsupported.
pie::Config parse_pie_init(std::string_view source);

}  // namespace mermaid::detail

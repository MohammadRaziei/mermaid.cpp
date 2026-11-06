// sequence_css.h (private) - the <style> text of a sequence diagram: Mermaid's common styles, the rules of
// diagrams/sequence/styles.ts and the neo look rules, compiled under `#<svg id>`.
#pragma once

#include <string>

#include "theme_variables.h"

namespace mermaid::detail {

std::string sequence_stylesheet(const theme_variables& theme, const std::string& svg_id, bool neo = true);

}  // namespace mermaid::detail

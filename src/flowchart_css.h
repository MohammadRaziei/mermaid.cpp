// flowchart_css.h (private) - the <style> text of a flowchart, assembled like Mermaid 12.2:
// common styles + flowchart styles + neo look rules + user styles (classDef), compiled under
// `#<svg id>`.
#pragma once

#include <string>

#include <mermaid/flowchart.h>

#include "theme_variables.h"

namespace mermaid::detail {

std::string flowchart_stylesheet(const theme_variables& theme, const std::string& svg_id, const flowchart::Model& model,
                                 const std::string& extra_css);

}  // namespace mermaid::detail

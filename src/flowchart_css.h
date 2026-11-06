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

// The shared frame of every diagram's stylesheet (Mermaid's styles.ts + globalStyles.ts) around `diagram_css`, the
// rules of the diagram type, and `user_css`; compiled under `#<svg id>`.
std::string assemble_stylesheet(const theme_variables& theme, const std::string& svg_id, const std::string& diagram_css,
                                const std::string& user_css);

}  // namespace mermaid::detail

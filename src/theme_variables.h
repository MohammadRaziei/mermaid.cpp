// theme_variables.h (private) - the resolved theme variables Mermaid's stylesheets read.
//
// Values are what Mermaid's own theme code computes (`getThemeVariables()` of the theme, run once
// with node); the colour arithmetic of khroma is not repeated here.
#pragma once

#include <string>
#include <vector>

namespace mermaid::detail {

struct theme_variables {
    std::string font_family;
    std::string font_size;
    std::string text_color;
    std::string error_bkg_color;
    std::string error_text_color;
    std::string line_color;
    std::string node_text_color;
    std::string main_bkg;
    std::string node_border;
    double stroke_width = 1;
    std::string arrowhead_color;
    std::string edge_label_background;
    std::string cluster_bkg;
    std::string cluster_border;
    std::string title_color;
    std::string tertiary_color;
    std::string border2;
    std::string drop_shadow;  // "url(#drop-shadow)" or empty
    bool use_gradient = false;
    int theme_color_limit = 0;
    std::vector<std::string> border_color_array;
    std::vector<std::string> bkg_color_array;
    std::string look = "neo";
    std::string font_family_config;  // `fontFamily` of the diagram config (the :root rule)

    // Mermaid 12.2 default: theme `redux-color`, look `neo`.
    static theme_variables redux_color();
};

}  // namespace mermaid::detail

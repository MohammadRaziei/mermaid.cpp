// theme_variables.h (private) - the resolved theme variables Mermaid's stylesheets read.
//
// Values are what Mermaid's own theme code computes (`getThemeVariables()` of the theme, run once
// with node); the colour arithmetic of khroma is not repeated here.
#pragma once

#include <string>
#include <vector>

#include <mermaid/flowchart.h>

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
    bool node_shadow = false;    // themeVariables.nodeShadow: the small shapes of the redux themes get a drop shadow
    std::string gradient_start;  // themeVariables.gradientStart (stop of the node border gradient)
    std::string gradient_stop;
    std::string look = "neo";
    std::string font_family_config;  // `fontFamily` of the diagram config (the :root rule)

    // Mermaid 12.2 default: theme `redux-color`, look `neo`.
    static theme_variables redux_color();
    // The classic themes in the neo look (what Mermaid 12.2 draws for `theme: dark` and so on).
    static theme_variables dark();
    static theme_variables forest();
    static theme_variables neutral();
    static theme_variables base();
    // Variables of a theme by name (`default`, `dark`, `forest`, `neutral`, `base`); throws std::invalid_argument.
    static theme_variables named(const std::string& name);
    // `named(init theme, or `fallback` without one)` with the themeVariables of the init directive on top.
    static theme_variables resolved(const flowchart::InitConfig& init, const std::string& fallback);
    // font_size as a number of pixels ("16px" -> 16)
    double font_px() const;
};

}  // namespace mermaid::detail

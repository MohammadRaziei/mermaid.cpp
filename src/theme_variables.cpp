#include "theme_variables.h"

#include <cstdlib>
#include <stdexcept>

namespace mermaid::detail {

theme_variables theme_variables::redux_color() {
    theme_variables t;
    t.node_shadow = true;
    t.font_family = "\"Recursive Variable\", arial, sans-serif";
    t.font_size = "14px";
    t.text_color = "#28253D";
    t.error_bkg_color = "#ffffff";
    t.error_text_color = "#000000";
    t.line_color = "#000000";
    t.node_text_color = "#28253D";
    t.main_bkg = "#ffffff";
    t.node_border = "#28253D";
    t.stroke_width = 2;
    t.arrowhead_color = "#000000";
    t.edge_label_background = "hsl(-120, 0%, 80%)";
    t.cluster_bkg = "#F9F9FB";
    t.cluster_border = "#BDBCCC";
    t.title_color = "#000000";
    t.tertiary_color = "#ffffff";
    t.border2 = "hsl(0, 0%, 90%)";
    t.drop_shadow = "url(#drop-shadow)";
    t.use_gradient = false;
    t.theme_color_limit = 12;
    t.border_color_array = {"#E879F9", "#2DD4BF", "#FB923C", "#22D3EE", "#4ADE80", "#A78BFA",
                            "#F87171", "#FACC15", "#818CF8", "#A3E635 ", "#38BDF8", "#FB7185"};
    t.bkg_color_array = {"#FDF4FF", "#F0FDFA", "#FFF7ED", "#ECFEFF", "#F0FDF4", "#F5F3FF",
                         "#FEF2F2", "#FEFCE8", "#EEF2FF", "#F7FEE7", "#F0F9FF", "#FFF1F2"};
    t.font_family_config = "\"trebuchet ms\", verdana, arial, sans-serif";
    return t;
}

theme_variables theme_variables::dark() {
    theme_variables t;
    t.font_family = "\"trebuchet ms\", verdana, arial, sans-serif";
    t.font_size = "16px";
    t.text_color = "#ccc";
    t.error_bkg_color = "#a44141";
    t.error_text_color = "#ddd";
    t.line_color = "lightgrey";
    t.node_text_color = "#ccc";
    t.main_bkg = "#1f2020";
    t.node_border = "#ccc";
    t.stroke_width = 1;
    t.arrowhead_color = "lightgrey";
    t.edge_label_background = "hsl(0, 0%, 34.4117647059%)";
    t.cluster_bkg = "hsl(180, 1.5873015873%, 28.3529411765%)";
    t.cluster_border = "rgba(255, 255, 255, 0.25)";
    t.title_color = "#F9FFFE";
    t.tertiary_color = "hsl(20, 1.5873015873%, 12.3529411765%)";
    t.border2 = "rgba(255, 255, 255, 0.25)";
    t.drop_shadow = "drop-shadow( 1px 2px 2px rgba(185,185,185,1))";
    t.use_gradient = true;
    t.gradient_start = "#cccccc";
    t.gradient_stop = "hsl(180, 0%, 18.3529411765%)";
    t.theme_color_limit = 12;
    t.font_family_config = "\"trebuchet ms\", verdana, arial, sans-serif";
    return t;
}

theme_variables theme_variables::forest() {
    theme_variables t;
    t.font_family = "\"trebuchet ms\", verdana, arial, sans-serif";
    t.font_size = "16px";
    t.text_color = "#000000";
    t.error_bkg_color = "#552222";
    t.error_text_color = "#552222";
    t.line_color = "#000000";
    t.node_text_color = "#000000";
    t.main_bkg = "#cde498";
    t.node_border = "#13540c";
    t.stroke_width = 1;
    t.arrowhead_color = "green";
    t.edge_label_background = "#e8e8e8";
    t.cluster_bkg = "#cdffb2";
    t.cluster_border = "#6eaa49";
    t.title_color = "#333";
    t.tertiary_color = "hsl(78.1578947368, 58.4615384615%, 84.5098039216%)";
    t.border2 = "#6eaa49";
    t.drop_shadow = "drop-shadow( 1px 2px 2px rgba(185,185,185,0.5))";
    t.use_gradient = true;
    t.gradient_start = "hsl(78.1578947368, 18.4615384615%, 64.5098039216%)";
    t.gradient_stop = "hsl(98.961038961, 60%, 74.9019607843%)";
    t.theme_color_limit = 12;
    t.font_family_config = "\"trebuchet ms\", verdana, arial, sans-serif";
    return t;
}

theme_variables theme_variables::neutral() {
    theme_variables t;
    t.font_family = "\"trebuchet ms\", verdana, arial, sans-serif";
    t.font_size = "16px";
    t.text_color = "#000000";
    t.error_bkg_color = "#552222";
    t.error_text_color = "#552222";
    t.line_color = "#666";
    t.node_text_color = "#000000";
    t.main_bkg = "#eee";
    t.node_border = "#999";
    t.stroke_width = 1;
    t.arrowhead_color = "#333333";
    t.edge_label_background = "white";
    t.cluster_bkg = "hsl(0, 0%, 98.9215686275%)";
    t.cluster_border = "#707070";
    t.title_color = "#333";
    t.tertiary_color = "hsl(-160, 0%, 93.3333333333%)";
    t.border2 = "#707070";
    t.drop_shadow = "drop-shadow( 1px 2px 2px rgba(185,185,185,1))";
    t.use_gradient = true;
    t.gradient_start = "hsl(0, 0%, 83.3333333333%)";
    t.gradient_stop = "hsl(0, 0%, 88.9215686275%)";
    t.theme_color_limit = 12;
    t.font_family_config = "\"trebuchet ms\", verdana, arial, sans-serif";
    return t;
}

theme_variables theme_variables::base() {
    theme_variables t;
    t.font_family = "\"trebuchet ms\", verdana, arial, sans-serif";
    t.font_size = "16px";
    t.text_color = "#333";
    t.error_bkg_color = "hsl(220.5882352941, 100%, 98.3333333333%)";
    t.error_text_color = "rgb(8.5000000002, 5.7500000001, 0)";
    t.line_color = "#0b0b0b";
    t.node_text_color = "#333";
    t.main_bkg = "#fff4dd";
    t.node_border = "hsl(40.5882352941, 60%, 83.3333333333%)";
    t.stroke_width = 1;
    t.arrowhead_color = "#0b0b0b";
    t.edge_label_background = "hsl(-79.4117647059, 100%, 93.3333333333%)";
    t.cluster_bkg = "hsl(220.5882352941, 100%, 98.3333333333%)";
    t.cluster_border = "hsl(220.5882352941, 60%, 88.3333333333%)";
    t.title_color = "rgb(8.5000000002, 5.7500000001, 0)";
    t.tertiary_color = "hsl(220.5882352941, 100%, 98.3333333333%)";
    t.border2 = "hsl(220.5882352941, 60%, 88.3333333333%)";
    t.drop_shadow = "drop-shadow( 1px 2px 2px rgba(185,185,185,1))";
    t.use_gradient = true;
    t.gradient_start = "hsl(40.5882352941, 60%, 83.3333333333%)";
    t.gradient_stop = "hsl(-79.4117647059, 60%, 83.3333333333%)";
    t.theme_color_limit = 12;
    t.font_family_config = "\"trebuchet ms\", verdana, arial, sans-serif";
    return t;
}

theme_variables theme_variables::named(const std::string& name) {
    if (name == "default") return redux_color();
    if (name == "dark") return dark();
    if (name == "forest") return forest();
    if (name == "neutral") return neutral();
    if (name == "base") return base();
    throw std::invalid_argument("unknown theme '" + name + "' (default, dark, forest, neutral, base)");
}

theme_variables theme_variables::resolved(const flowchart::InitConfig& init, const std::string& fallback) {
    theme_variables vars = named(init.theme ? *init.theme : fallback);
    if (init.font_family) vars.font_family = *init.font_family;
    if (init.cluster_bkg) vars.cluster_bkg = *init.cluster_bkg;
    if (init.cluster_border) vars.cluster_border = *init.cluster_border;
    return vars;
}

double theme_variables::font_px() const { return std::atof(font_size.c_str()); }

}  // namespace mermaid::detail

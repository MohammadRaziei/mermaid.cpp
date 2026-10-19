#include "theme_variables.h"

namespace mermaid::detail {

theme_variables theme_variables::redux_color() {
    theme_variables t;
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

}  // namespace mermaid::detail

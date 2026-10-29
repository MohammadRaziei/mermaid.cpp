#include "theme_variables.h"

#include <cstdlib>
#include <functional>
#include <map>
#include <stdexcept>

#include "khroma.h"

namespace mermaid::detail {
namespace {

using namespace khroma;

// What the theme classes of Mermaid (src/themes/theme-*.js) do with `this`: a bag of strings where JavaScript's `a = a || b`
// assigns `b` only when `a` is missing or empty. Only the variables the flowchart stylesheet reads, and those they derive from,
// are kept.
class theme_state {
public:
    std::string get(const std::string& key) const {
        const auto it = values_.find(key);
        return it == values_.end() ? std::string() : it->second;
    }
    void set(const std::string& key, std::string value) { values_[key] = std::move(value); }
    // `this.key = this.key || value()`; the value is only worked out when needed (a bad colour only throws then).
    void otherwise(const std::string& key, const std::function<std::string()>& value) {
        if (get(key).empty()) set(key, value());
    }
    bool dark_mode() const { return !get("darkMode").empty() && get("darkMode") != "false"; }

private:
    std::map<std::string, std::string> values_;
};

// theme.calculate(overrides): the overrides go in, updateColors() derives what is missing (and may overwrite), the overrides
// go in again.
void calculate(theme_state& state, const std::map<std::string, std::string>& overrides, const std::function<void(theme_state&)>& update) {
    for (const auto& [key, value] : overrides) state.set(key, value);
    update(state);
    for (const auto& [key, value] : overrides) state.set(key, value);
}

// The part of updateColors() shared by theme-base.js and theme-redux-color.js; they differ in the default text colour.
void update_generic(theme_state& s, const char* primary_text) {
    const bool dark = s.dark_mode();
    s.otherwise("primaryTextColor", [&] { return dark ? "#eee" : primary_text; });
    s.otherwise("secondaryColor", [&] { return adjust(s.get("primaryColor"), {{'h', -120}}); });
    s.otherwise("tertiaryColor", [&] { return adjust(s.get("primaryColor"), {{'h', 180}, {'l', 5}}); });
    s.otherwise("primaryBorderColor", [&] { return mk_border(s.get("primaryColor"), dark); });
    s.otherwise("secondaryBorderColor", [&] { return mk_border(s.get("secondaryColor"), dark); });
    s.otherwise("tertiaryBorderColor", [&] { return mk_border(s.get("tertiaryColor"), dark); });
    s.otherwise("secondaryTextColor", [&] { return invert(s.get("secondaryColor")); });
    s.otherwise("tertiaryTextColor", [&] { return invert(s.get("tertiaryColor")); });
    s.otherwise("lineColor", [&] { return invert(s.get("background")); });
    s.otherwise("arrowheadColor", [&] { return invert(s.get("background")); });
    s.otherwise("textColor", [&] { return s.get("primaryTextColor"); });
    s.otherwise("border2", [&] { return s.get("tertiaryBorderColor"); });
    s.otherwise("nodeBkg", [&] { return s.get("primaryColor"); });
    s.otherwise("mainBkg", [&] { return s.get("primaryColor"); });
    s.otherwise("nodeBorder", [&] { return s.get("primaryBorderColor"); });
    s.otherwise("clusterBkg", [&] { return s.get("tertiaryColor"); });
    s.otherwise("clusterBorder", [&] { return s.get("tertiaryBorderColor"); });
    s.otherwise("titleColor", [&] { return s.get("tertiaryTextColor"); });
    s.otherwise("edgeLabelBackground", [&] { return dark ? darken(s.get("secondaryColor"), 30) : s.get("secondaryColor"); });
    s.otherwise("nodeTextColor", [&] { return s.get("primaryTextColor"); });
    s.otherwise("errorBkgColor", [&] { return s.get("tertiaryColor"); });
    s.otherwise("errorTextColor", [&] { return s.get("tertiaryTextColor"); });
}

// theme-redux-color.js: the default theme of Mermaid 12.2 (theme `default` with look `neo`).
theme_state redux_color_state(const std::map<std::string, std::string>& overrides) {
    theme_state s;
    s.set("background", "#ffffff");
    s.set("primaryColor", "#cccccc");
    s.set("mainBkg", "#ffffff");
    s.set("THEME_COLOR_LIMIT", "12");
    s.set("strokeWidth", "2");
    s.set("primaryBorderColor", mk_border("#28253D", false));
    s.set("fontFamily", "\"Recursive Variable\", arial, sans-serif");
    s.set("fontSize", "14px");
    s.set("nodeBorder", "#28253D");
    s.set("useGradient", "");
    s.set("dropShadow", "url(#drop-shadow)");
    s.set("nodeShadow", "true");
    s.set("tertiaryColor", "#ffffff");
    s.set("clusterBkg", "#F9F9FB");
    s.set("clusterBorder", "#BDBCCC");
    calculate(s, overrides, [](theme_state& t) { update_generic(t, "#28253D"); });
    return s;
}

// theme-base.js
theme_state base_state(const std::map<std::string, std::string>& overrides) {
    theme_state s;
    s.set("background", "#f4f4f4");
    s.set("primaryColor", "#fff4dd");
    s.set("THEME_COLOR_LIMIT", "12");
    s.set("strokeWidth", "1");
    s.set("fontFamily", "\"trebuchet ms\", verdana, arial, sans-serif");
    s.set("fontSize", "16px");
    s.set("useGradient", "true");
    s.set("dropShadow", "drop-shadow( 1px 2px 2px rgba(185,185,185,1))");
    calculate(s, overrides, [](theme_state& t) {
        update_generic(t, "#333");
        t.set("gradientStart", t.get("primaryBorderColor"));
        t.set("gradientStop", t.get("secondaryBorderColor"));
    });
    // A node border of the user's own makes the gradient pointless.
    if (overrides.count("nodeBorder") && !overrides.count("useGradient")) s.set("useGradient", "");
    return s;
}

// theme-dark.js
theme_state dark_state(const std::map<std::string, std::string>& overrides) {
    theme_state s;  // the constructor runs before any override, so `darkMode` is still undefined in it
    s.set("background", "#333");
    s.set("primaryColor", "#1f2020");
    s.set("secondaryColor", lighten(s.get("primaryColor"), 16));
    s.set("tertiaryColor", adjust(s.get("primaryColor"), {{'h', -160}}));
    s.set("primaryBorderColor", invert(s.get("background")));
    s.set("secondaryBorderColor", mk_border(s.get("secondaryColor"), false));
    s.set("tertiaryBorderColor", mk_border(s.get("tertiaryColor"), false));
    s.set("primaryTextColor", invert(s.get("primaryColor")));
    s.set("secondaryTextColor", invert(s.get("secondaryColor")));
    s.set("tertiaryTextColor", invert(s.get("tertiaryColor")));
    s.set("textColor", invert(s.get("background")));
    s.set("mainBkg", "#1f2020");
    s.set("mainContrastColor", "lightgrey");
    s.set("border1", "#ccc");
    s.set("border2", rgba(255, 255, 255, 0.25));
    s.set("fontFamily", "\"trebuchet ms\", verdana, arial, sans-serif");
    s.set("fontSize", "16px");
    s.set("labelBackground", "#181818");
    s.set("textColor", "#ccc");
    s.set("THEME_COLOR_LIMIT", "12");
    s.set("strokeWidth", "1");
    s.set("titleColor", "#F9FFFE");
    s.set("errorBkgColor", "#a44141");
    s.set("errorTextColor", "#ddd");
    s.set("useGradient", "true");
    s.set("gradientStart", s.get("primaryBorderColor"));
    s.set("gradientStop", s.get("secondaryBorderColor"));
    s.set("dropShadow", "drop-shadow( 1px 2px 2px rgba(185,185,185,1))");
    calculate(s, overrides, [](theme_state& t) {
        t.set("secondBkg", lighten(t.get("mainBkg"), 16));
        t.set("lineColor", t.get("mainContrastColor"));
        t.set("arrowheadColor", t.get("mainContrastColor"));
        t.set("nodeBkg", t.get("mainBkg"));
        t.set("nodeBorder", t.get("border1"));
        t.set("clusterBkg", t.get("secondBkg"));
        t.set("clusterBorder", t.get("border2"));
        t.set("edgeLabelBackground", lighten(t.get("labelBackground"), 25));
        t.otherwise("errorBkgColor", [&] { return t.get("tertiaryColor"); });
        t.otherwise("errorTextColor", [&] { return t.get("tertiaryTextColor"); });
        t.otherwise("nodeBorder", [] { return std::string("#999"); });
    });
    return s;
}

// theme-forest.js
theme_state forest_state(const std::map<std::string, std::string>& overrides) {
    theme_state s;
    s.set("primaryColor", "#cde498");
    s.set("secondaryColor", "#cdffb2");
    s.set("background", "white");
    s.set("mainBkg", "#cde498");
    s.set("secondBkg", "#cdffb2");
    s.set("lineColor", "green");
    s.set("border1", "#13540c");
    s.set("border2", "#6eaa49");
    s.set("arrowheadColor", "green");
    s.set("fontFamily", "\"trebuchet ms\", verdana, arial, sans-serif");
    s.set("fontSize", "16px");
    s.set("tertiaryColor", lighten("#cde498", 10));
    s.set("primaryBorderColor", mk_border(s.get("primaryColor"), false));
    s.set("secondaryBorderColor", mk_border(s.get("secondaryColor"), false));
    s.set("tertiaryBorderColor", mk_border(s.get("tertiaryColor"), false));
    s.set("primaryTextColor", invert(s.get("primaryColor")));
    s.set("secondaryTextColor", invert(s.get("secondaryColor")));
    s.set("tertiaryTextColor", invert(s.get("primaryColor")));
    s.set("lineColor", invert(s.get("background")));
    s.set("textColor", invert(s.get("background")));
    s.set("THEME_COLOR_LIMIT", "12");
    s.set("strokeWidth", "1");
    s.set("titleColor", "#333");
    s.set("edgeLabelBackground", "#e8e8e8");
    s.set("errorBkgColor", "#552222");
    s.set("errorTextColor", "#552222");
    s.set("useGradient", "true");
    s.set("gradientStart", s.get("primaryBorderColor"));
    s.set("gradientStop", s.get("secondaryBorderColor"));
    s.set("dropShadow", "drop-shadow( 1px 2px 2px rgba(185,185,185,0.5))");
    calculate(s, overrides, [](theme_state& t) {
        t.set("nodeBkg", t.get("mainBkg"));
        t.set("nodeBorder", t.get("border1"));
        t.set("clusterBkg", t.get("secondBkg"));
        t.set("clusterBorder", t.get("border2"));
        t.otherwise("errorBkgColor", [&] { return t.get("tertiaryColor"); });
        t.otherwise("errorTextColor", [&] { return t.get("tertiaryTextColor"); });
    });
    return s;
}

// theme-neutral.js
theme_state neutral_state(const std::map<std::string, std::string>& overrides) {
    theme_state s;
    s.set("primaryColor", "#eee");
    s.set("contrast", "#707070");
    s.set("secondaryColor", lighten(s.get("contrast"), 55));
    s.set("background", "#ffffff");
    s.set("tertiaryColor", adjust(s.get("primaryColor"), {{'h', -160}}));
    s.set("primaryBorderColor", mk_border(s.get("primaryColor"), false));
    s.set("secondaryBorderColor", mk_border(s.get("secondaryColor"), false));
    s.set("tertiaryBorderColor", mk_border(s.get("tertiaryColor"), false));
    s.set("primaryTextColor", invert(s.get("primaryColor")));
    s.set("secondaryTextColor", invert(s.get("secondaryColor")));
    s.set("tertiaryTextColor", invert(s.get("tertiaryColor")));
    s.set("textColor", invert(s.get("background")));
    s.set("mainBkg", "#eee");
    s.set("lineColor", "#666");
    s.set("border1", "#999");
    s.set("text", "#333");
    s.set("arrowheadColor", "#333333");
    s.set("fontFamily", "\"trebuchet ms\", verdana, arial, sans-serif");
    s.set("fontSize", "16px");
    s.set("THEME_COLOR_LIMIT", "12");
    s.set("strokeWidth", "1");
    s.set("edgeLabelBackground", "white");
    s.set("errorBkgColor", "#552222");
    s.set("errorTextColor", "#552222");
    s.set("useGradient", "true");
    s.set("gradientStart", s.get("primaryBorderColor"));
    s.set("gradientStop", s.get("secondaryBorderColor"));
    s.set("dropShadow", "drop-shadow( 1px 2px 2px rgba(185,185,185,1))");
    calculate(s, overrides, [](theme_state& t) {
        t.set("secondBkg", lighten(t.get("contrast"), 55));
        t.set("border2", t.get("contrast"));
        t.set("nodeBkg", t.get("mainBkg"));
        t.set("nodeBorder", t.get("border1"));
        t.set("clusterBkg", t.get("secondBkg"));
        t.set("clusterBorder", t.get("border2"));
        t.set("titleColor", t.get("text"));
        t.otherwise("errorBkgColor", [&] { return t.get("tertiaryColor"); });
        t.otherwise("errorTextColor", [&] { return t.get("tertiaryTextColor"); });
    });
    return s;
}

// What the stylesheets of Mermaid read from the theme (`options.x`): `nodeTextColor` falls back to `textColor` there.
theme_variables read_variables(const theme_state& s, bool redux) {
    theme_variables t;
    t.font_family = s.get("fontFamily");
    t.font_size = s.get("fontSize");
    t.text_color = s.get("textColor");
    t.error_bkg_color = s.get("errorBkgColor");
    t.error_text_color = s.get("errorTextColor");
    t.line_color = s.get("lineColor");
    t.node_text_color = s.get("nodeTextColor").empty() ? s.get("textColor") : s.get("nodeTextColor");
    t.main_bkg = s.get("mainBkg");
    t.node_border = s.get("nodeBorder");
    t.stroke_width = std::atof(s.get("strokeWidth").c_str());
    t.arrowhead_color = s.get("arrowheadColor");
    t.edge_label_background = s.get("edgeLabelBackground");
    t.cluster_bkg = s.get("clusterBkg");
    t.cluster_border = s.get("clusterBorder");
    t.title_color = s.get("titleColor");
    t.tertiary_color = s.get("tertiaryColor");
    t.border2 = s.get("border2");
    t.drop_shadow = s.get("dropShadow");
    t.use_gradient = !s.get("useGradient").empty() && s.get("useGradient") != "false";
    t.gradient_start = s.get("gradientStart");
    t.gradient_stop = s.get("gradientStop");
    t.theme_color_limit = std::atoi(s.get("THEME_COLOR_LIMIT").c_str());
    t.node_shadow = !s.get("nodeShadow").empty() && s.get("nodeShadow") != "false";
    t.font_family_config = "\"trebuchet ms\", verdana, arial, sans-serif";
    if (redux) {
        t.border_color_array = {"#E879F9", "#2DD4BF", "#FB923C", "#22D3EE", "#4ADE80", "#A78BFA",
                                "#F87171", "#FACC15", "#818CF8", "#A3E635 ", "#38BDF8", "#FB7185"};
        t.bkg_color_array = {"#FDF4FF", "#F0FDFA", "#FFF7ED", "#ECFEFF", "#F0FDF4", "#F5F3FF",
                             "#FEF2F2", "#FEFCE8", "#EEF2FF", "#F7FEE7", "#F0F9FF", "#FFF1F2"};
    }
    return t;
}

}  // namespace

theme_variables theme_variables::named(const std::string& name) { return resolved_theme(name, {}); }

theme_variables theme_variables::resolved_theme(const std::string& name, const std::map<std::string, std::string>& overrides) {
    if (name == "default") return read_variables(redux_color_state(overrides), true);
    if (name == "dark") return read_variables(dark_state(overrides), false);
    if (name == "forest") return read_variables(forest_state(overrides), false);
    if (name == "neutral") return read_variables(neutral_state(overrides), false);
    if (name == "base") return read_variables(base_state(overrides), false);
    throw std::invalid_argument("unknown theme '" + name + "' (default, dark, forest, neutral, base)");
}

theme_variables theme_variables::resolved(const flowchart::InitConfig& init, const std::string& fallback) {
    theme_variables t = resolved_theme(init.theme ? *init.theme : fallback, init.theme_variables);
    if (init.font_family) t.font_family_config = *init.font_family;
    return t;
}

double theme_variables::font_px() const { return std::atof(font_size.c_str()); }

}  // namespace mermaid::detail

#include "flowchart_css.h"

#include <map>

#include <mermaid/css_color.h>
#include <mermaid/css_compiler.h>

namespace mermaid::detail {

namespace {

// Templates in the order of Mermaid's `styles.ts`, `diagrams/flowchart/styles.ts` and
// `diagrams/globalStyles.ts`; `@name@` marks a theme value.
const char* const k_common = R"CSS(
& {
    font-family: @fontFamily@;
    font-size: @fontSize@;
    fill: @textColor@
  }
  @keyframes edge-animation-frame {
    from {
      stroke-dashoffset: 0;
    }
  }
  @keyframes dash {
    to {
      stroke-dashoffset: 0;
    }
  }
  & .edge-animation-slow {
    stroke-dasharray: 9,5 !important;
    stroke-dashoffset: 900;
    animation: dash 50s linear infinite;
    stroke-linecap: round;
  }
  & .edge-animation-fast {
    stroke-dasharray: 9,5 !important;
    stroke-dashoffset: 900;
    animation: dash 20s linear infinite;
    stroke-linecap: round;
  }
  & .error-icon {
    fill: @errorBkgColor@;
  }
  & .error-text {
    fill: @errorTextColor@;
    stroke: @errorTextColor@;
  }
  & .edge-thickness-normal {
    stroke-width: @strokeWidth@px;
  }
  & .edge-thickness-thick {
    stroke-width: 3.5px
  }
  & .edge-pattern-solid {
    stroke-dasharray: 0;
  }
  & .edge-thickness-invisible {
    stroke-width: 0;
    fill: none;
  }
  & .edge-pattern-dashed{
    stroke-dasharray: 3;
  }
  .edge-pattern-dotted {
    stroke-dasharray: 2;
  }
  & .marker {
    fill: @lineColor@;
    stroke: @lineColor@;
  }
  & .marker.cross {
    stroke: @lineColor@;
  }
  & svg {
    font-family: @fontFamily@;
    font-size: @fontSize@;
  }
   & p {
    margin: 0
   }
  @DIAGRAM@
  .node .neo-node {
    stroke: @nodeBorder@;
  }
  [data-look="neo"].node rect, [data-look="neo"].cluster rect, [data-look="neo"].node polygon {
    stroke: @nodeStroke@;
    filter: @dropShadow@;
  }
  [data-look="neo"].swimlane.cluster rect {
    filter: none;
  }
  [data-look="neo"].node path {
    stroke: @nodeStroke@;
    stroke-width: @strokeWidth@px;
  }
  [data-look="neo"].node .outer-path {
    filter: @dropShadow@;
  }
  [data-look="neo"].node .neo-line path {
    stroke: @nodeBorder@;
    filter: none;
  }
  [data-look="neo"].node circle{
    stroke: @nodeStroke@;
    filter: @dropShadow@;
  }
  [data-look="neo"].node circle .state-start{
    fill: #000000;
  }
  [data-look="neo"].icon-shape .icon {
    fill: @nodeStroke@;
    filter: @dropShadow@;
  }
  [data-look="neo"].icon-shape .icon-neo path {
    stroke: @nodeStroke@;
    filter: @dropShadow@;
  }
  @USER@
)CSS";
const char* const k_flowchart = R"CSS(
@COLORS@
  .label {
    font-family: @fontFamily@;
    color: @nodeText@;
  }
  .cluster-label text {
    fill: @titleColor@;
  }
  .cluster-label span {
    color: @titleColor@;
  }
  .cluster-label span p {
    background-color: transparent;
  }
  .label text,span {
    fill: @nodeText@;
    color: @nodeText@;
  }
  .node rect,
  .node circle,
  .node ellipse,
  .node polygon,
  .node path {
    fill: @mainBkg@;
    stroke: @nodeBorder@;
    stroke-width: @strokeWidth@px;
  }
  .rough-node .label text , .node .label text, .image-shape .label, .icon-shape .label {
    text-anchor: middle;
  }
  .node .katex path {
    fill: #000;
    stroke: #000;
    stroke-width: 1px;
  }
  .rough-node .label,.node .label, .image-shape .label, .icon-shape .label {
    text-align: center;
  }
  .node.clickable {
    cursor: pointer;
  }
  .root .anchor path {
    fill: @lineColor@ !important;
    stroke-width: 0;
    stroke: @lineColor@;
  }
  .arrowheadPath {
    fill: @arrowheadColor@;
  }
  .edgePaths .path {
    stroke: @lineColor@;
    stroke-width: @strokeWidth@px;
  }
  .flowchart-link {
    stroke: @lineColor@;
    fill: none;
  }
  .edgeLabel {
    background-color: @edgeLabelBackground@;
    p {
      background-color: @edgeLabelBackground@;
    }
    rect {
      opacity: 0.5;
      background-color: @edgeLabelBackground@;
      fill: @edgeLabelBackground@;
    }
    text-align: center;
  }
  .labelBkg {
    background-color: @fadedEdgeLabelBackground@;
  }
  .cluster rect {
    fill: @clusterBkg@;
    stroke: @clusterBorder@;
    stroke-width: 1px;
  }
  .cluster text {
    fill: @titleColor@;
  }
  .cluster span {
    color: @titleColor@;
  }
  .node .collapsed-indicator {
    fill: @clusterBorder@;
    stroke: none;
    opacity: 0.6;
  }
  .node .collapsed-separator {
    stroke: @clusterBorder@;
    stroke-width: 0.75px;
  }
  div.mermaidTooltip {
    position: absolute;
    text-align: center;
    max-width: 200px;
    padding: 2px;
    font-family: @fontFamily@;
    font-size: 12px;
    background: @tertiaryColor@;
    border: 1px solid @border2@;
    border-radius: 2px;
    pointer-events: none;
    z-index: 100;
  }
  .flowchartTitleText {
    text-anchor: middle;
    font-size: 18px;
    fill: @textColor@;
  }
  rect.text {
    fill: none;
    stroke-width: 0;
  }
  .icon-shape, .image-shape {
    background-color: @edgeLabelBackground@;
    p {
      background-color: @edgeLabelBackground@;
      padding: 2px;
    }
    .label rect {
      opacity: 0.5;
      background-color: @edgeLabelBackground@;
      fill: @edgeLabelBackground@;
    }
    text-align: center;
  }
  .label-icon {
    display: inline-block;
    height: 1em;
    overflow: visible;
    vertical-align: -0.125em;
  }
  .node .label-icon path {
    fill: currentColor;
    stroke: revert;
    stroke-width: revert;
  }
)CSS";
const char* const k_color_slot = R"CSS(
    @SLOT@.cluster:not(.swimlane) rect {
      stroke: @BORDER@;
      fill: @FILL@;
    }
    @SLOT@.cluster:not(.swimlane) path {
      stroke: @BORDER@;
      fill: @FILL@;
    }
    @SLOT@.swimlane.cluster rect.swimlane-title,
    @SLOT@.swimlane.cluster rect.swimlane-body {
      stroke: @BORDER@;
      fill: @FILL@;
    }
    @SLOT@.swimlane.cluster .swimlane-title path:nth-of-type(2), @SLOT@.swimlane.cluster .swimlane-body path:nth-of-type(2) {
      stroke: @BORDER@;
    }
    @SLOT@.swimlane.cluster .swimlane-title path:first-of-type, @SLOT@.swimlane.cluster .swimlane-body path:first-of-type {
      stroke: @FILL@;
    }
    @SLOT@.node .collapsed-group, @SLOT@.rough-node .collapsed-group,
    @SLOT@.node .collapsed-group path, @SLOT@.rough-node .collapsed-group path {
      stroke: @BORDER@;
      fill: @FILL@;
    }
    @SLOT@.node .collapsed-indicator, @SLOT@.rough-node .collapsed-indicator {
      fill: @BORDER@;
    }
    @SLOT@.node .collapsed-separator, @SLOT@.rough-node .collapsed-separator {
      stroke: @BORDER@;
    }
)CSS";

std::string substitute(std::string text, const std::map<std::string, std::string>& values) {
    for (const auto& [name, value] : values) {
        const std::string token = "@" + name + "@";
        for (size_t at = text.find(token); at != std::string::npos; at = text.find(token, at + value.size()))
            text.replace(at, token.size(), value);
    }
    return text;
}

std::string number_text(double v) {
    std::string s = std::to_string(v);
    s.erase(s.find_last_not_of('0') + 1);
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

// genColor: the per-subgraph palette (only for the colour themes).
std::string color_sections(const theme_variables& t) {
    if (t.border_color_array.empty()) return {};
    const bool has_bkg = !t.bkg_color_array.empty();
    const int count = t.theme_color_limit > 0 ? t.theme_color_limit : static_cast<int>(t.border_color_array.size());
    std::string out;
    for (int i = 0; i < count; ++i) {
        const std::string border = t.border_color_array[static_cast<size_t>(i) % t.border_color_array.size()];
        const std::string fill = has_bkg ? t.bkg_color_array[static_cast<size_t>(i) % t.bkg_color_array.size()] : std::string();
        std::string section = substitute(k_color_slot, {{"SLOT", "[data-look=\"" + t.look + "\"][data-color-id=\"color-" + std::to_string(i) + "\"]"},
                                                        {"BORDER", border},
                                                        {"FILL", fill}});
        out += section;
    }
    return out;
}

std::string trim_blanks(const std::string& value) {
    const size_t a = value.find_first_not_of(" \t");
    return a == std::string::npos ? std::string() : value.substr(a, value.find_last_not_of(" \t") - a + 1);
}

// Splits at blanks and/or commas (CSS list separators), dropping empty pieces.
std::vector<std::string> split_list(const std::string& value, bool commas_only) {
    std::vector<std::string> out;
    std::string current;
    const auto flush = [&] {
        const std::string piece = trim_blanks(current);
        if (!piece.empty()) out.push_back(piece);
        current.clear();
    };
    for (const char c : value) {
        if (c == ',' || (!commas_only && (c == ' ' || c == '\t'))) flush();
        else current += c;
    }
    flush();
    return out;
}

// What a browser prints for a style value read back from the CSSOM (Mermaid inserts the classDef rules into a
// CSSStyleSheet): hex colours become rgb(), `stroke-dasharray` is a comma list, multi-word font names are quoted.
std::string browser_value(const std::string& key, const std::string& value) {
    const std::string v = trim_blanks(value);
    if (key == "stroke-dasharray") {
        std::string out;
        for (const std::string& item : split_list(v, false)) out += (out.empty() ? "" : ", ") + item;
        return out.empty() ? v : out;
    }
    if (key == "font-family") {
        std::string out;
        for (std::string family : split_list(v, true)) {
            const bool quoted = family.front() == '"' || family.front() == '\'';
            if (!quoted && family.find_first_of(" \t") != std::string::npos) family = "\"" + family + "\"";
            out += (out.empty() ? "" : ", ") + family;
        }
        return out.empty() ? v : out;
    }
    const std::optional<rgb_color> c = v.empty() || v[0] != '#' ? std::nullopt : parse_css_color(v);
    return c ? css_rgb(*c) : v;
}

std::string important_rule(const std::string& class_name, const std::string& element, const std::vector<std::string>& declarations) {
    std::string body;
    for (const std::string& d : declarations) body += d + " !important; ";
    return "." + class_name + " " + element + " { " + body + "}\n";
}

// createCssStyles: the :root font rule, then one rule per classDef element.
std::string user_styles(const theme_variables& t, const flowchart::Model& model, const std::string& extra_css) {
    std::string out = ":root { --mermaid-font-family: " + t.font_family_config + "}\n";
    for (const std::string& name : model.class_def_order) {
        const auto it = model.class_defs.find(name);
        if (it == model.class_defs.end() || it->second.style.empty()) continue;
        std::vector<std::string> all, text;
        for (const auto& [key, value] : it->second.style) {
            all.push_back(key + ": " + browser_value(key, value));
            if (key == "color") text.push_back("fill: " + browser_value(key, value));
        }
        if ((model.config.html_labels ? model.config.html_labels : model.config.flowchart_html_labels).value_or(true)) {
            out += important_rule(name, "> *", all);
            out += important_rule(name, "span", all);
        } else {  // createCssStyles with SVG labels: the shapes themselves
            for (const char* element : {"rect", "polygon", "ellipse", "circle", "path"}) out += important_rule(name, element, all);
        }
        if (!text.empty()) out += important_rule(name, "tspan", text);
    }
    return out + extra_css;
}

}  // namespace

std::string flowchart_stylesheet(const theme_variables& t, const std::string& svg_id, const flowchart::Model& model,
                                 const std::string& extra_css) {
    const std::string ns = "#" + svg_id;  // Mermaid passes the id with its '#'
    const std::string stroke_width = number_text(t.stroke_width);
    const std::string node_stroke = t.use_gradient ? "url(" + ns + "-gradient)" : t.node_border;
    std::string drop_shadow = "none";
    if (!t.drop_shadow.empty()) {
        drop_shadow = t.drop_shadow;
        const std::string from = "url(#drop-shadow)";
        const size_t at = drop_shadow.find(from);
        if (at != std::string::npos) drop_shadow.replace(at, from.size(), "url(" + ns + "-drop-shadow)");
    }
    const std::string node_text = t.node_text_color.empty() ? t.text_color : t.node_text_color;

    const std::string flowchart = substitute(k_flowchart, {{"COLORS", color_sections(t)},
                                                           {"fontFamily", t.font_family},
                                                           {"nodeText", node_text},
                                                           {"titleColor", t.title_color},
                                                           {"mainBkg", t.main_bkg},
                                                           {"nodeBorder", t.node_border},
                                                           {"strokeWidth", stroke_width},
                                                           {"lineColor", t.line_color},
                                                           {"arrowheadColor", t.arrowhead_color},
                                                           {"edgeLabelBackground", t.edge_label_background},
                                                           {"fadedEdgeLabelBackground", fade_color(t.edge_label_background, 0.5)},
                                                           {"clusterBkg", t.cluster_bkg},
                                                           {"clusterBorder", t.cluster_border},
                                                           {"tertiaryColor", t.tertiary_color},
                                                           {"border2", t.border2},
                                                           {"textColor", t.text_color}});
    const std::string all = substitute(k_common, {{"fontFamily", t.font_family},
                                                  {"fontSize", t.font_size},
                                                  {"textColor", t.text_color},
                                                  {"errorBkgColor", t.error_bkg_color},
                                                  {"errorTextColor", t.error_text_color},
                                                  {"strokeWidth", stroke_width},
                                                  {"lineColor", t.line_color},
                                                  {"nodeBorder", t.node_border},
                                                  {"nodeStroke", node_stroke},
                                                  {"dropShadow", drop_shadow},
                                                  {"DIAGRAM", flowchart},
                                                  {"USER", user_styles(t, model, extra_css)}});
    return css_compiler::compile(ns, all);
}

}  // namespace mermaid::detail

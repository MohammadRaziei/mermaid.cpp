#include "sequence_css.h"

#include <map>

#include "flowchart_css.h"

namespace mermaid::detail {

namespace {

// diagrams/sequence/styles.ts; `@name@` marks a theme value.
const char* const k_sequence = R"CSS(
  .actor {
    stroke: @actorBorder@;
    fill: @actorBkg@;
    stroke-width: @strokeWidth@;
  }
  rect.actor.outer-path[data-look="neo"] {
      filter: @dropShadow@;
  }
  rect.note[data-look="neo"] {
      stroke:@noteBorderColor@;
      fill:@noteBkgColor@;
      filter: @dropShadow@;
  }
  text.actor > tspan {
    fill: @actorTextColor@;
    stroke: none;
  }
  .actor-line {
    stroke: @actorLineColor@;
  }
  .innerArc {
    stroke-width: 1.5;
    stroke-dasharray: none;
  }
  .messageLine0 {
    stroke-width: 1.5;
    stroke-dasharray: none;
    stroke: @signalColor@;
  }
  .messageLine1 {
    stroke-width: 1.5;
    stroke-dasharray: 2, 2;
    stroke: @signalColor@;
  }
  [id$="-arrowhead"] path {
    fill: @signalColor@;
    stroke: @signalColor@;
  }
  .sequenceNumber {
    fill: @sequenceNumberColor@;
  }
  [id$="-sequencenumber"] {
    fill: @signalColor@;
  }
  [id$="-crosshead"] path {
    fill: @signalColor@;
    stroke: @signalColor@;
  }
  .messageText {
    fill: @signalTextColor@;
    stroke: none;
  }
  .labelBox {
    stroke: @labelBoxBorderColor@;
    fill: @labelBoxBkgColor@;
    filter: @labelFilter@;
  }
  .labelText, .labelText > tspan {
    fill: @labelTextColor@;
    stroke: none;
  }
  .loopText, .loopText > tspan {
    fill: @loopTextColor@;
    stroke: none;
  }
  .sectionTitle, .sectionTitle > tspan {
    fill: @loopTextColor@;
    stroke: none;
  }
  .loopLine {
    stroke-width: 2px;
    stroke-dasharray: 2, 2;
    stroke: @labelBoxBorderColor@;
    fill: @labelBoxBorderColor@;
  }
  .note {
    stroke: @noteBorderColor@;
    fill: @noteBkgColor@;
  }
  .noteText, .noteText > tspan {
    fill: @noteTextColor@;
    stroke: none;
  }
  .activation0 {
    fill: @activationBkgColor@;
    stroke: @activationBorderColor@;
  }
  .activation1 {
    fill: @activationBkgColor@;
    stroke: @activationBorderColor@;
  }
  .activation2 {
    fill: @activationBkgColor@;
    stroke: @activationBorderColor@;
  }
  .actorPopupMenu {
    position: absolute;
  }
  .actorPopupMenuPanel {
    position: absolute;
    fill: @actorBkg@;
    box-shadow: 0px 8px 16px 0px rgba(0,0,0,0.2);
    filter: drop-shadow(3px 5px 2px rgb(0 0 0 / 0.4));
}
  .actor-man circle, line {
    fill: @actorBkg@;
    stroke-width: 2px;
  }
  g rect.rect {
    filter: @dropShadow@;
    stroke: @nodeBorder@;
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

}  // namespace

std::string sequence_stylesheet(const theme_variables& t, const std::string& svg_id, bool neo) {
    const std::string drop_shadow = t.drop_shadow.empty() ? "none" : t.drop_shadow;
    const auto v = [&](const char* key) { return t.sequence.at(key); };
    const std::string css = substitute(k_sequence, {{"actorBorder", v("actorBorder")},
                                                    {"actorBkg", v("actorBkg")},
                                                    {"strokeWidth", std::to_string(static_cast<int>(t.stroke_width))},
                                                    {"dropShadow", drop_shadow},
                                                    {"labelFilter", neo ? drop_shadow : "none"},
                                                    {"noteBorderColor", v("noteBorderColor")},
                                                    {"noteBkgColor", v("noteBkgColor")},
                                                    {"actorTextColor", v("actorTextColor")},
                                                    {"actorLineColor", v("actorLineColor")},
                                                    {"signalColor", v("signalColor")},
                                                    {"signalTextColor", v("signalTextColor")},
                                                    {"sequenceNumberColor", v("sequenceNumberColor")},
                                                    {"labelBoxBorderColor", v("labelBoxBorderColor")},
                                                    {"labelBoxBkgColor", v("labelBoxBkgColor")},
                                                    {"labelTextColor", v("labelTextColor")},
                                                    {"loopTextColor", v("loopTextColor")},
                                                    {"noteTextColor", v("noteTextColor")},
                                                    {"activationBkgColor", v("activationBkgColor")},
                                                    {"activationBorderColor", v("activationBorderColor")},
                                                    {"nodeBorder", t.node_border}});
    return assemble_stylesheet(t, svg_id, css, ":root { --mermaid-font-family: " + t.font_family_config + "}\n");
}

}  // namespace mermaid::detail

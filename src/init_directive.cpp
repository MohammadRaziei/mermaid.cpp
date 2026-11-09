#include "init_directive.h"
#include "json.h"
#include "rough_path.h"

#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mermaid::detail {

namespace {


std::optional<std::string> string_at(const json_value& obj, const std::string& group, const std::string& key) {
    const json_value* g = obj.find(group);
    const json_value* v = g && g->type == json_value::kind::object ? g->find(key) : nullptr;
    if (v && v->type == json_value::kind::string) return v->string;
    return std::nullopt;
}

std::optional<std::string> top_string(const json_value& obj, const std::string& key) {
    const json_value* v = obj.find(key);
    if (v && v->type == json_value::kind::string) return v->string;
    return std::nullopt;
}

std::optional<double> number_at(const json_value& obj, const std::string& group, const std::string& key) {
    const json_value* g = obj.find(group);
    const json_value* v = g && g->type == json_value::kind::object ? g->find(key) : nullptr;
    if (v && v->type == json_value::kind::number) return v->number;
    return std::nullopt;
}

// config.ts sanitizeDirective / sanitize, as far as `themeVariables` goes. A directive key must be one Mermaid's default config
// knows (`configKeys`); the five variables below are read by our themes but absent from it, so the directive cannot set them
// (found by asking Mermaid 12.2.0). A string value may only hold digits, space, quotes, `#%(),.;` and letters, else it is
// emptied; `<`, `>` and `url(data:` remove the key.
bool theme_variable_allowed(const std::string& key, const json_value& v) {
    static const char* const unknown[] = {"mainContrastColor", "contrast", "text", "nodeTextColor", "nodeShadow"};
    for (const char* name : unknown)
        if (key == name) return false;
    if (key.rfind("__", 0) == 0 || key.find("proto") != std::string::npos || key.find("constr") != std::string::npos) return false;
    if (v.type == json_value::kind::string &&
        (v.string.find('<') != std::string::npos || v.string.find('>') != std::string::npos || v.string.find("url(data:") != std::string::npos))
        return false;
    return true;
}

std::string sanitized_theme_value(const std::string& value) {
    if (value.empty()) return value;
    for (const char c : value)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '"' || c == '#' || c == '%' || c == '(' || c == ')' || c == ',' || c == '.' || c == ';'))
            return std::string();
    return value;
}

template <typename T>
void override_with(std::optional<T>& target, const std::optional<T>& value) {
    if (value) target = value;
}

// The text after `init:` / `initialize:` inside a directive body, or empty.
std::string_view directive_payload(std::string_view body) {
    size_t i = 0;
    while (i < body.size() && std::isspace(static_cast<unsigned char>(body[i]))) ++i;
    size_t word_end = i;
    while (word_end < body.size() && std::isalpha(static_cast<unsigned char>(body[word_end]))) ++word_end;
    const std::string_view word = body.substr(i, word_end - i);
    if (word != "init" && word != "initialize") return {};
    size_t colon = word_end;
    while (colon < body.size() && std::isspace(static_cast<unsigned char>(body[colon]))) ++colon;
    if (colon >= body.size() || body[colon] != ':') return {};
    return body.substr(colon + 1);
}

// theme, themeVariables, fontFamily and look of one directive: what the stylesheet of any diagram type derives its colours from.
void read_theme_settings(const json_value& value_ref, flowchart::InitConfig& config) {
    const json_value* value = &value_ref;
        override_with(config.theme, top_string(*value, "theme"));
        std::map<std::string, std::string> variables_here;
        if (const json_value* variables = value->find("themeVariables"); variables && variables->type == json_value::kind::object)
            for (const auto& [key, v] : variables->members) {
                if (!theme_variable_allowed(key, v)) continue;
                if (v.type == json_value::kind::string) variables_here[key] = sanitized_theme_value(v.string);
                else if (v.type == json_value::kind::number) variables_here[key] = js_number(v.number);
                else if (v.type == json_value::kind::boolean) variables_here[key] = v.number != 0 ? "true" : "";
            }
        // addDirective: a plain `fontFamily` becomes themeVariables.fontFamily unless the directive sets that itself
        if (const auto font = top_string(*value, "fontFamily"); font && variables_here[std::string("fontFamily")].empty())
            variables_here["fontFamily"] = *font;
        override_with(config.font_family, top_string(*value, "fontFamily"));
        for (const auto& [key, v] : variables_here) config.theme_variables[key] = v;  // assignWithDepth: a later directive wins key by key
    if (const json_value* look = value->find("look"); look && look->type == json_value::kind::string) config.look = look->string;
}

}  // namespace

flowchart::InitConfig parse_init_directives(std::string_view source) {
    flowchart::InitConfig config;
    size_t pos = 0;
    while ((pos = source.find("%%{", pos)) != std::string_view::npos) {
        const size_t end = source.find("}%%", pos);
        if (end == std::string_view::npos) break;
        const std::string_view body = source.substr(pos + 3, end - pos - 3);
        pos = end + 3;

        const std::string_view payload = directive_payload(body);
        if (payload.empty()) continue;
        // The closing `}` of the payload object is part of `}%%`'s neighbour: the body lacks one brace.
        const std::string text = std::string(payload) + "}";
        auto value = json_reader(payload).read_object();
        if (!value) value = json_reader(text).read_object();
        if (!value) continue;

        read_theme_settings(*value, config);
        override_with(config.node_spacing, number_at(*value, "flowchart", "nodeSpacing"));
        override_with(config.rank_spacing, number_at(*value, "flowchart", "rankSpacing"));
        if (const json_value* flowchart = value->find("flowchart"); flowchart && flowchart->type == json_value::kind::object)
            if (const json_value* pad = flowchart->find("padding"); pad && pad->type == json_value::kind::number) config.padding = pad->number;
        if (const json_value* flowchart = value->find("flowchart"); flowchart && flowchart->type == json_value::kind::object) {
            if (const json_value* pad = flowchart->find("diagramPadding"); pad && pad->type == json_value::kind::number) config.diagram_padding = pad->number;
            if (const json_value* wrap = flowchart->find("wrappingWidth"); wrap && wrap->type == json_value::kind::number) config.wrapping_width = wrap->number;
            if (const json_value* wide = flowchart->find("useMaxWidth"); wide && wide->type == json_value::kind::boolean) config.use_max_width = wide->number != 0;
        }
        override_with(config.curve, string_at(*value, "flowchart", "curve"));
        if (const json_value* html = value->find("htmlLabels"); html && html->type == json_value::kind::boolean)
            config.html_labels = html->number != 0;
        if (const json_value* flowchart = value->find("flowchart"); flowchart && flowchart->type == json_value::kind::object)
            if (const json_value* nested = flowchart->find("htmlLabels"); nested && nested->type == json_value::kind::boolean)
                config.flowchart_html_labels = nested->number != 0;
    }
    return config;
}

sequence::Config parse_sequence_init(std::string_view source) {
    sequence::Config config;
    flowchart::InitConfig theme;
    size_t pos = 0;
    while ((pos = source.find("%%{", pos)) != std::string_view::npos) {
        const size_t end = source.find("}%%", pos);
        if (end == std::string_view::npos) break;
        const std::string_view body = source.substr(pos + 3, end - pos - 3);
        pos = end + 3;
        const std::string_view payload = directive_payload(body);
        if (payload.empty()) continue;
        const std::string text = std::string(payload) + "}";
        auto value = json_reader(payload).read_object();
        if (!value) value = json_reader(text).read_object();
        if (!value) continue;
        read_theme_settings(*value, theme);
        if (const json_value* wrap = value->find("wrap"); wrap && wrap->type == json_value::kind::boolean) config.wrap = wrap->number != 0;
        if (const json_value* size = value->find("fontSize")) {
            // "14px" or "14": utils.parseFontSize; any other unit is left to the browser
            std::string digits = size->type == json_value::kind::string ? size->string : std::string();
            if (digits.size() > 2 && digits.compare(digits.size() - 2, 2, "px") == 0) digits.resize(digits.size() - 2);
            char* end = nullptr;
            const double parsed = digits.empty() ? 0 : std::strtod(digits.c_str(), &end);
            if (size->type == json_value::kind::number) config.font_size = size->number;
            else if (!digits.empty() && end && *end == '\0') config.font_size = parsed;
            else config.unsupported.push_back("fontSize");
        }
        const json_value* group = value->find("sequence");
        if (!group || group->type != json_value::kind::object) continue;
        struct number_key {
            const char* name;
            std::optional<double> sequence::Config::*field;
        };
        static const number_key numbers[] = {
            {"width", &sequence::Config::width},
            {"height", &sequence::Config::height},
            {"actorMargin", &sequence::Config::actor_margin},
            {"boxMargin", &sequence::Config::box_margin},
            {"boxTextMargin", &sequence::Config::box_text_margin},
            {"noteMargin", &sequence::Config::note_margin},
            {"messageMargin", &sequence::Config::message_margin},
            {"wrapPadding", &sequence::Config::wrap_padding},
            {"diagramMarginX", &sequence::Config::diagram_margin_x},
            {"diagramMarginY", &sequence::Config::diagram_margin_y},
            {"bottomMarginAdj", &sequence::Config::bottom_margin_adj},
            {"activationWidth", &sequence::Config::activation_width},
            {"labelBoxWidth", &sequence::Config::label_box_width},
            {"labelBoxHeight", &sequence::Config::label_box_height},
        };
        for (const number_key& k : numbers)
            if (const json_value* v = group->find(k.name); v && v->type == json_value::kind::number) config.*k.field = v->number;
        struct bool_key {
            const char* name;
            std::optional<bool> sequence::Config::*field;
        };
        static const bool_key flags[] = {
            {"mirrorActors", &sequence::Config::mirror_actors},
            {"wrap", &sequence::Config::wrap},
            {"showSequenceNumbers", &sequence::Config::show_sequence_numbers},
            {"useMaxWidth", &sequence::Config::use_max_width},
            {"rightAngles", &sequence::Config::right_angles},
            {"hideUnusedParticipants", &sequence::Config::hide_unused_participants},
        };
        for (const bool_key& k : flags)
            if (const json_value* v = group->find(k.name); v && v->type == json_value::kind::boolean) config.*k.field = v->number != 0;
        struct string_key {
            const char* name;
            std::optional<std::string> sequence::Config::*field;
        };
        static const string_key strings[] = {{"messageAlign", &sequence::Config::message_align}, {"noteAlign", &sequence::Config::note_align}};
        for (const string_key& k : strings)
            if (const json_value* v = group->find(k.name); v && v->type == json_value::kind::string) config.*k.field = v->string;
        static const char* const undrawn[] = {"actorFontFamily", "actorFontWeight", "noteFontFamily", "noteFontWeight", "messageFontFamily", "messageFontWeight"};
        for (const char* name : undrawn)
            if (group->find(name)) config.unsupported.push_back(name);
    }
    config.theme = theme.theme;
    config.font_family = theme.font_family;
    config.theme_variables = theme.theme_variables;
    config.look = theme.look;
    return config;
}

pie::Config parse_pie_init(std::string_view source) {
    pie::Config config;
    const auto unsupported = [&](const char* key) {
        if (std::find(config.unsupported.begin(), config.unsupported.end(), key) == config.unsupported.end()) config.unsupported.push_back(key);
    };
    size_t pos = 0;
    while ((pos = source.find("%%{", pos)) != std::string_view::npos) {
        const size_t end = source.find("}%%", pos);
        if (end == std::string_view::npos) break;
        const std::string_view body = source.substr(pos + 3, end - pos - 3);
        pos = end + 3;
        const std::string_view payload = directive_payload(body);
        if (payload.empty()) continue;
        const std::string text = std::string(payload) + "}";
        auto value = json_reader(payload).read_object();
        if (!value) value = json_reader(text).read_object();
        if (!value) continue;
        if (const json_value* theme = value->find("theme"); theme && !(theme->type == json_value::kind::string && theme->string == "default")) unsupported("theme");
        if (value->find("themeVariables")) unsupported("themeVariables");
        if (value->find("fontFamily")) unsupported("fontFamily");
        const json_value* group = value->find("pie");
        if (!group || group->type != json_value::kind::object) continue;
        if (const json_value* v = group->find("textPosition"); v && v->type == json_value::kind::number) config.text_position = v->number;
        if (const json_value* v = group->find("donutHole"); v && v->type == json_value::kind::number) config.donut_hole = v->number;
        if (const json_value* v = group->find("legendPosition"); v && v->type == json_value::kind::string) config.legend_position = v->string;
        if (const json_value* v = group->find("highlightSlice"); v && v->type == json_value::kind::string) config.highlight_slice = v->string;
        if (const json_value* v = group->find("useMaxWidth"); v && v->type == json_value::kind::boolean) config.use_max_width = v->number != 0;
    }
    return config;
}

}  // namespace mermaid::detail

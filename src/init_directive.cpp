#include "init_directive.h"
#include "json.h"
#include "rough_path.h"

#include <cctype>
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
        override_with(config.node_spacing, number_at(*value, "flowchart", "nodeSpacing"));
        override_with(config.rank_spacing, number_at(*value, "flowchart", "rankSpacing"));
        override_with(config.curve, string_at(*value, "flowchart", "curve"));
        if (const json_value* html = value->find("htmlLabels"); html && html->type == json_value::kind::boolean)
            config.html_labels = html->number != 0;
        if (const json_value* flowchart = value->find("flowchart"); flowchart && flowchart->type == json_value::kind::object)
            if (const json_value* nested = flowchart->find("htmlLabels"); nested && nested->type == json_value::kind::boolean)
                config.flowchart_html_labels = nested->number != 0;
    }
    return config;
}

}  // namespace mermaid::detail

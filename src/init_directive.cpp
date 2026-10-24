#include "init_directive.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mermaid::detail {

namespace {

// A JSON value, just enough for configuration objects.
struct json_value {
    enum class kind { null, boolean, number, string, array, object } type = kind::null;
    double number = 0;
    std::string string;
    std::map<std::string, json_value> members;

    const json_value* find(const std::string& key) const {
        const auto it = members.find(key);
        return it == members.end() ? nullptr : &it->second;
    }
};

class json_reader {
public:
    explicit json_reader(std::string_view text) : s_(text) {}

    // The object starting at the first non-blank character, or nullopt when the text is not valid JSON.
    std::optional<json_value> read_object() {
        skip_space();
        if (pos_ >= s_.size() || s_[pos_] != '{') return std::nullopt;
        json_value v;
        if (!read_value(v, 0)) return std::nullopt;
        return v;
    }

private:
    static constexpr int k_max_depth = 32;

    void skip_space() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    bool consume(char c) {
        skip_space();
        if (pos_ < s_.size() && s_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool read_literal(std::string_view word) {
        if (s_.compare(pos_, word.size(), word) != 0) return false;
        pos_ += word.size();
        return true;
    }

    bool read_string(std::string& out) {
        if (!consume('"')) return false;
        out.clear();
        while (pos_ < s_.size()) {
            const char c = s_[pos_++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (pos_ >= s_.size()) return false;
            const char e = s_[pos_++];
            switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u':  // keep the escape as written; no configuration value here needs it decoded
                    out += "\\u";
                    break;
                default: out += e; break;
            }
        }
        return false;
    }

    bool read_value(json_value& v, int depth) {
        if (depth > k_max_depth) return false;
        skip_space();
        if (pos_ >= s_.size()) return false;
        const char c = s_[pos_];
        if (c == '{') {
            ++pos_;
            v.type = json_value::kind::object;
            if (consume('}')) return true;
            while (true) {
                std::string key;
                if (!read_string(key) || !consume(':')) return false;
                if (!read_value(v.members[key], depth + 1)) return false;
                if (consume(',')) continue;
                return consume('}');
            }
        }
        if (c == '[') {
            ++pos_;
            v.type = json_value::kind::array;
            if (consume(']')) return true;
            while (true) {
                json_value ignored;
                if (!read_value(ignored, depth + 1)) return false;
                if (consume(',')) continue;
                return consume(']');
            }
        }
        if (c == '"') {
            v.type = json_value::kind::string;
            return read_string(v.string);
        }
        if (read_literal("true") || read_literal("false")) {
            v.type = json_value::kind::boolean;
            return true;
        }
        if (read_literal("null")) return true;
        const char* begin = s_.data() + pos_;
        char* end = nullptr;
        const double d = std::strtod(begin, &end);
        if (end == begin) return false;
        v.type = json_value::kind::number;
        v.number = d;
        pos_ += static_cast<size_t>(end - begin);
        return true;
    }

    std::string_view s_;
    size_t pos_ = 0;
};

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
        override_with(config.font_family, string_at(*value, "themeVariables", "fontFamily"));
        override_with(config.cluster_bkg, string_at(*value, "themeVariables", "clusterBkg"));
        override_with(config.cluster_border, string_at(*value, "themeVariables", "clusterBorder"));
        override_with(config.node_spacing, number_at(*value, "flowchart", "nodeSpacing"));
        override_with(config.rank_spacing, number_at(*value, "flowchart", "rankSpacing"));
        override_with(config.curve, string_at(*value, "flowchart", "curve"));
    }
    return config;
}

}  // namespace mermaid::detail

// json.h (private) - a small JSON reader: enough for configuration objects and the golden dumps of the tests.
#pragma once

#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mermaid::detail {

// A JSON value, just enough for configuration objects.
struct json_value {
    enum class kind { null, boolean, number, string, array, object } type = kind::null;
    double number = 0;
    std::string string;
    std::map<std::string, json_value> members;  // objects
    std::vector<std::string> order;              // the keys of an object in the order they were written (JavaScript's)
    std::vector<json_value> items;               // arrays

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
                if (!v.members.count(key)) v.order.push_back(key);
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
                json_value item;
                if (!read_value(item, depth + 1)) return false;
                v.items.push_back(std::move(item));
                if (consume(',')) continue;
                return consume(']');
            }
        }
        if (c == '"') {
            v.type = json_value::kind::string;
            return read_string(v.string);
        }
        if (read_literal("true")) {
            v.type = json_value::kind::boolean;
            v.number = 1;
            return true;
        }
        if (read_literal("false")) {
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

}  // namespace mermaid::detail

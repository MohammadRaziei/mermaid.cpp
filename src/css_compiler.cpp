#include <mermaid/css_compiler.h>

#include <cctype>
#include <vector>

namespace mermaid {

namespace {

bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string strip_comments(const std::string& css) {
    std::string out;
    for (size_t i = 0; i < css.size();) {
        if (css.compare(i, 2, "/*") == 0) {
            const size_t end = css.find("*/", i + 2);
            i = end == std::string::npos ? css.size() : end + 2;
        } else if (css.compare(i, 2, "//") == 0 && (i == 0 || css[i - 1] != ':')) {
            const size_t end = css.find('\n', i);
            i = end == std::string::npos ? css.size() : end;
        } else {
            out += css[i++];
        }
    }
    return out;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Splits at top-level `,` (outside (), [] and quotes).
std::vector<std::string> split_list(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    int depth = 0;
    char quote = 0;
    for (char c : s) {
        if (quote) {
            cur += c;
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') quote = c;
        if (c == '(' || c == '[') ++depth;
        if (c == ')' || c == ']') --depth;
        if (c == ',' && depth == 0) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

// Collapses whitespace and removes it around the combinators `>`, `+` and `~`.
std::string normalize_selector(const std::string& raw) {
    std::string out;
    char quote = 0;
    int bracket = 0;
    for (size_t i = 0; i < raw.size(); ++i) {
        const char c = raw[i];
        if (quote) {
            out += c;
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            out += c;
        } else if (c == '[') {
            ++bracket;
            out += c;
        } else if (c == ']') {
            --bracket;
            out += c;
        } else if (is_space(c) && bracket == 0) {
            if (!out.empty() && out.back() != ' ' && out.back() != '>' && out.back() != '+' && out.back() != '~') out += ' ';
        } else if ((c == '>' || c == '+' || c == '~') && bracket == 0) {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            out += c;
        } else {
            out += c;
        }
    }
    return trim(out);
}

// `prop: value` -> `prop:value`: spaces after commas (outside parentheses and quotes) and before
// `!important` disappear, runs of spaces collapse.
std::string normalize_declaration(const std::string& raw) {
    const size_t colon = raw.find(':');
    if (colon == std::string::npos) return {};
    const std::string prop = trim(raw.substr(0, colon));
    const std::string value = trim(raw.substr(colon + 1));
    std::string out;
    int depth = 0;
    char quote = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (quote) {
            out += c;
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            out += c;
        } else if (c == '(') {
            ++depth;
            out += c;
        } else if (c == ')') {
            --depth;
            out += c;
        } else if (is_space(c)) {
            if (out.empty() || out.back() == ' ') continue;
            if (depth == 0 && out.back() == ',') continue;
            out += ' ';
        } else if (c == '!' && depth == 0) {
            while (!out.empty() && out.back() == ' ') out.pop_back();
            out += c;
        } else {
            out += c;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return prop + ":" + out + ";";
}

size_t matching_brace(const std::string& s, size_t open) {
    int depth = 0;
    char quote = 0;
    for (size_t i = open; i < s.size(); ++i) {
        const char c = s[i];
        if (quote) {
            if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') quote = c;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) return i;
    }
    return s.size();
}

std::vector<std::string> expand(const std::vector<std::string>& parents, const std::string& selector_text) {
    std::vector<std::string> out;
    for (const std::string& part_raw : split_list(selector_text)) {
        const std::string part = normalize_selector(part_raw);
        if (part.empty()) continue;
        for (const std::string& parent : parents) {
            if (part.find('&') == std::string::npos) {
                out.push_back(parent.empty() ? part : parent + " " + part);
            } else {
                std::string replaced;
                for (char c : part) {
                    if (c == '&') replaced += parent;
                    else replaced += c;
                }
                out.push_back(replaced);
            }
        }
    }
    return out;
}

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (size_t i = 0; i < items.size(); ++i) out += (i ? "," : "") + items[i];
    return out;
}

std::string compile_block(const std::vector<std::string>& selectors, const std::string& body);

// At-rules with inner selectors that must stay as written (`@keyframes`).
std::string compile_at_rule(const std::string& head, const std::string& body) {
    std::string out = normalize_selector(head) + "{";
    size_t i = 0;
    while (i < body.size()) {
        const size_t open = body.find('{', i);
        if (open == std::string::npos) break;
        const size_t close = matching_brace(body, open);
        std::string decls;
        const std::string inner_body = body.substr(open + 1, close - open - 1);
        size_t start = 0;
        while (start < inner_body.size()) {
            const size_t semi = inner_body.find(';', start);
            const std::string piece = inner_body.substr(start, semi == std::string::npos ? semi : semi - start);
            if (!trim(piece).empty()) decls += normalize_declaration(piece);
            if (semi == std::string::npos) break;
            start = semi + 1;
        }
        out += normalize_selector(body.substr(i, open - i)) + "{" + decls + "}";
        i = close + 1;
    }
    return out + "}";
}

std::string compile_block(const std::vector<std::string>& selectors, const std::string& body) {
    std::string declarations;
    std::string nested;
    std::string buf;
    size_t i = 0;
    while (i < body.size()) {
        const char c = body[i];
        if (c == ';') {
            declarations += normalize_declaration(buf);
            buf.clear();
            ++i;
        } else if (c == '{') {
            const size_t close = matching_brace(body, i);
            const std::string inner = body.substr(i + 1, close - i - 1);
            const std::string head = trim(buf);
            if (!head.empty() && head[0] == '@') nested += compile_at_rule(head, inner);
            else nested += compile_block(expand(selectors, head), inner);
            buf.clear();
            i = close + 1;
        } else {
            buf += c;
            ++i;
        }
    }
    if (!trim(buf).empty()) declarations += normalize_declaration(buf);
    std::string out;
    if (!declarations.empty()) out += join(selectors) + "{" + declarations + "}";
    return out + nested;
}

}  // namespace

std::string css_compiler::compile(const std::string& ns, const std::string& css) {
    return compile_block({ns}, strip_comments(css));
}

}  // namespace mermaid

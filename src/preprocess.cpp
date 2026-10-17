#include "preprocess.h"

#include <cctype>

namespace mermaid::detail {

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

namespace {

// Blanks out `[from, to)` but keeps newlines so line numbers stay correct.
void blank(std::string& s, size_t from, size_t to) {
    for (size_t i = from; i < to && i < s.size(); ++i)
        if (s[i] != '\n') s[i] = ' ';
}

void strip_frontmatter(std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) ++i;
    if (s.compare(i, 3, "---") != 0) return;
    size_t eol = s.find('\n', i);
    if (eol == std::string::npos || trim(s.substr(i, eol - i)) != "---") return;
    size_t pos = eol + 1;
    while (pos < s.size()) {
        size_t next = s.find('\n', pos);
        std::string line = trim(s.substr(pos, next == std::string::npos ? std::string::npos : next - pos));
        if (line == "---") {
            blank(s, i, next == std::string::npos ? s.size() : next);
            return;
        }
        if (next == std::string::npos) return;
        pos = next + 1;
    }
}

void strip_directives(std::string& s) {
    size_t pos = 0;
    while ((pos = s.find("%%{", pos)) != std::string::npos) {
        size_t end = s.find("}%%", pos);
        size_t stop = end == std::string::npos ? s.size() : end + 3;
        blank(s, pos, stop);
        pos = stop;
    }
}

}  // namespace

std::vector<Statement> split_statements(std::string_view source) {
    std::string s(source);
    strip_frontmatter(s);
    strip_directives(s);

    std::vector<Statement> out;
    std::string cur;
    int line = 1, cur_line = 1, depth = 0;
    bool in_quote = false;

    auto flush = [&]() {
        std::string t = trim(cur);
        if (!t.empty()) out.push_back({t, cur_line});
        cur.clear();
        depth = 0;
    };

    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '\r') continue;
        if (c == '\n') {
            in_quote = false;
            flush();
            ++line;
            cur_line = line;
            continue;
        }
        if (!in_quote && c == '%' && i + 1 < s.size() && s[i + 1] == '%') {
            while (i < s.size() && s[i] != '\n') ++i;
            --i;  // let the loop see the newline
            continue;
        }
        if (cur.empty() && std::isspace(static_cast<unsigned char>(c))) {
            cur_line = line;
            continue;
        }
        if (c == '"') in_quote = !in_quote;
        if (!in_quote) {
            if (c == '[' || c == '(' || c == '{') ++depth;
            else if ((c == ']' || c == ')' || c == '}') && depth > 0) --depth;
            else if (c == ';' && depth == 0) {
                flush();
                cur_line = line;
                continue;
            }
        }
        cur += c;
    }
    flush();
    return out;
}

}  // namespace mermaid::detail

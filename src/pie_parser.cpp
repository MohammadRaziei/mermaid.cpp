#include <mermaid/pie_parser.h>

#include <cctype>
#include <string>
#include <vector>

#include <mermaid/error.h>

#include "init_directive.h"
#include "preprocess.h"

namespace mermaid::pie {

namespace {

using detail::trim;

// `word` at the start of `s`, followed by a blank or the end: the rest after the blanks
bool take_keyword(const std::string& s, const std::string& word, std::string& rest) {
    if (s.compare(0, word.size(), word) != 0) return false;
    if (s.size() > word.size() && !std::isspace(static_cast<unsigned char>(s[word.size()]))) return false;
    rest = trim(s.substr(word.size()));
    return true;
}

// YAML frontmatter `title: ...` (a plain or quoted scalar), which Mermaid sets as the title before the text is parsed
std::string frontmatter_title(std::string_view source) {
    size_t pos = 0;
    while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos]))) ++pos;
    if (source.compare(pos, 3, "---") != 0) return {};
    pos = source.find('\n', pos);
    while (pos != std::string_view::npos && pos < source.size()) {
        const size_t next = source.find('\n', pos + 1);
        const std::string line = trim(source.substr(pos + 1, next == std::string_view::npos ? std::string_view::npos : next - pos - 1));
        if (line == "---") break;
        std::string rest;
        if (line.rfind("title:", 0) == 0) {
            rest = trim(line.substr(6));
            if (rest.size() >= 2 && (rest.front() == '"' || rest.front() == '\'') && rest.back() == rest.front()) rest = rest.substr(1, rest.size() - 2);
            return rest;
        }
        pos = next;
    }
    return {};
}

// `"label" : 12.5` -> true; the label has no quote inside and the value is an unsigned decimal
bool read_section(const std::string& s, Section& out) {
    if (s.empty() || s[0] != '"') return false;
    const size_t close = s.find('"', 1);
    if (close == std::string::npos || close == 1) return false;
    const std::string label = trim(s.substr(1, close - 1));
    std::string rest = trim(s.substr(close + 1));
    if (rest.empty() || rest[0] != ':') return false;
    rest = trim(rest.substr(1));
    // (0|[1-9][0-9]*)(\.[0-9]+)?
    size_t i = 0;
    if (i < rest.size() && rest[i] == '0') ++i;
    else if (i < rest.size() && rest[i] >= '1' && rest[i] <= '9') while (i < rest.size() && std::isdigit(static_cast<unsigned char>(rest[i]))) ++i;
    else return false;
    if (i < rest.size() && rest[i] == '.') {
        const size_t digits = i + 1;
        i = digits;
        while (i < rest.size() && std::isdigit(static_cast<unsigned char>(rest[i]))) ++i;
        if (i == digits) return false;
    }
    if (i != rest.size()) return false;
    out.label = label;
    out.value = std::stod(rest);
    return true;
}

}  // namespace

Model Parser::parse(std::string_view source) const {
    Model model;
    model.config = detail::parse_pie_init(source);
    model.title = frontmatter_title(source);
    const std::vector<detail::Statement> statements = detail::split_statements(source);
    bool header = false;
    bool in_descr = false;
    std::string descr;
    for (const detail::Statement& statement : statements) {
        const std::string& s = statement.text;
        if (in_descr) {  // accDescr { ... }
            const size_t close = s.find('}');
            const std::string line = trim(s.substr(0, close));
            if (!line.empty()) descr += (descr.empty() ? "" : "\n") + line;
            if (close != std::string::npos) {
                model.acc_descr = descr;
                in_descr = false;
            }
            continue;
        }
        std::string rest;
        if (!header) {
            if (!take_keyword(s, "pie", rest)) throw ParseError("a pie chart starts with `pie`", statement.line);
            header = true;
            if (take_keyword(rest, "showData", rest)) model.show_data = true;
            if (take_keyword(rest, "title", rest)) model.title = rest;
            else if (!rest.empty()) throw ParseError("unexpected `" + rest + "` after `pie`", statement.line);
            continue;
        }
        if (take_keyword(s, "title", rest)) {
            model.title = rest;
        } else if (s.rfind("accTitle", 0) == 0 && trim(s.substr(8)).rfind(':', 0) == 0) {
            model.acc_title = trim(trim(s.substr(8)).substr(1));
        } else if (s.rfind("accDescr", 0) == 0 && !trim(s.substr(8)).empty()) {
            const std::string after = trim(s.substr(8));
            if (after[0] == ':') {
                model.acc_descr = trim(after.substr(1));
            } else if (after[0] == '{') {
                const size_t close = after.find('}');
                if (close != std::string::npos) {
                    model.acc_descr = trim(after.substr(1, close - 1));
                } else {
                    in_descr = true;
                    descr = trim(after.substr(1));
                }
            } else {
                throw ParseError("unexpected `" + s + "`", statement.line);
            }
        } else {
            Section section;
            if (!read_section(s, section)) throw ParseError("expected `\"label\" : value`, got `" + s + "`", statement.line);
            bool known = false;
            for (const Section& other : model.sections) known = known || other.label == section.label;
            if (!known) model.sections.push_back(section);
        }
    }
    if (!header) throw ParseError("empty diagram", 1);
    return model;
}

}  // namespace mermaid::pie

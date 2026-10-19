#include <mermaid/flowchart_parser.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <optional>

#include <mermaid/error.h>

#include "preprocess.h"

namespace mermaid::flowchart {

namespace {

using detail::Statement;
using detail::trim;

bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

bool is_id_char(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_' || u >= 0x80;
}

bool starts_with_word(const std::string& s, const char* word) {
    size_t n = std::char_traits<char>::length(word);
    return s.compare(0, n, word) == 0 && (s.size() == n || is_space(s[n]));
}

std::string rest_after(const std::string& s, const char* word) {
    return trim(s.substr(std::char_traits<char>::length(word)));
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

void append_utf8(std::string& out, unsigned long cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// `<br>` -> newline, `#quot;` / `#35;` entities -> characters.
std::string decode_label(const std::string& raw) {
    std::string s = raw;
    // <br>, <br/>, <br /> (case-insensitive)
    std::string out;
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '<' && i + 3 < s.size() + 1 && lower(s.substr(i, 3)) == "<br") {
            size_t j = i + 3;
            while (j < s.size() && is_space(s[j])) ++j;
            if (j < s.size() && s[j] == '/') ++j;
            while (j < s.size() && is_space(s[j])) ++j;
            if (j < s.size() && s[j] == '>') {
                out += '\n';
                i = j + 1;
                continue;
            }
        }
        out += s[i++];
    }
    s.swap(out);
    out.clear();
    static const std::map<std::string, std::string> named = {
        {"quot", "\""}, {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"nbsp", " "}, {"apos", "'"}};
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '#') {
            size_t semi = s.find(';', i + 1);
            if (semi != std::string::npos && semi - i <= 10) {
                std::string name = s.substr(i + 1, semi - i - 1);
                auto it = named.find(name);
                if (it != named.end()) {
                    out += it->second;
                    i = semi + 1;
                    continue;
                }
                if (!name.empty() && std::all_of(name.begin(), name.end(), [](char c) {
                        return std::isdigit(static_cast<unsigned char>(c)) != 0;
                    })) {
                    append_utf8(out, std::strtoul(name.c_str(), nullptr, 10));
                    i = semi + 1;
                    continue;
                }
            }
        }
        out += s[i++];
    }
    return out;
}

// Quotes and markdown ticks around a whole label are dropped.
std::string clean_label(const std::string& text) {
    std::string t = trim(text);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"') t = t.substr(1, t.size() - 2);
    if (t.size() >= 2 && t.front() == '`' && t.back() == '`') t = t.substr(1, t.size() - 2);
    return decode_label(t);
}

std::optional<Direction> parse_direction(const std::string& word) {
    if (word == "TD" || word == "TB") return Direction::TB;
    if (word == "BT") return Direction::BT;
    if (word == "LR") return Direction::LR;
    if (word == "RL") return Direction::RL;
    return std::nullopt;
}

Shape shape_from_data_name(const std::string& name) {
    static const std::map<std::string, Shape> table = {
        {"rect", Shape::Square},       {"rectangle", Shape::Square},   {"square", Shape::Square},
        {"proc", Shape::Square},       {"process", Shape::Square},     {"rounded", Shape::Round},
        {"round", Shape::Round},       {"event", Shape::Round},        {"stadium", Shape::Stadium},
        {"pill", Shape::Stadium},      {"terminal", Shape::Stadium},   {"subproc", Shape::Subroutine},
        {"subroutine", Shape::Subroutine}, {"fr-rect", Shape::Subroutine}, {"cyl", Shape::Cylinder},
        {"cylinder", Shape::Cylinder}, {"db", Shape::Cylinder},        {"database", Shape::Cylinder},
        {"circle", Shape::Circle},     {"circ", Shape::Circle},        {"dbl-circ", Shape::DoubleCircle},
        {"doublecircle", Shape::DoubleCircle}, {"diam", Shape::Diamond}, {"diamond", Shape::Diamond},
        {"decision", Shape::Diamond},  {"question", Shape::Diamond},   {"hex", Shape::Hexagon},
        {"hexagon", Shape::Hexagon},   {"prepare", Shape::Hexagon},    {"odd", Shape::Odd},
        {"lean-r", Shape::LeanRight},  {"lean-l", Shape::LeanLeft},    {"trap-b", Shape::Trapezoid},
        {"trapezoid", Shape::Trapezoid}, {"trap-t", Shape::InvTrapezoid}, {"inv-trapezoid", Shape::InvTrapezoid},
    };
    auto it = table.find(lower(name));
    return it == table.end() ? Shape::Default : it->second;
}

struct EdgeSpec {
    std::string id;  // user supplied
    std::string label;
    Stroke stroke = Stroke::Normal;
    Head start = Head::None;
    Head end = Head::Arrow;
    int length = 1;
};

class Builder {
public:
    Model model;

    Node& upsert_node(const std::string& id) {
        auto it = index_.find(id);
        if (it != index_.end()) return model.nodes[it->second];
        index_[id] = model.nodes.size();
        Node n;
        n.id = id;
        n.label = id;
        n.dom_index = vertex_counter_;
        model.nodes.push_back(std::move(n));
        return model.nodes.back();
    }

    // Mermaid's addVertex: a node mentioned in a statement (or by `style`). Every call advances the
    // counter that names the DOM ids, whether or not the node is new.
    Node& mention_node(const std::string& id) {
        Node& node = upsert_node(id);
        ++vertex_counter_;
        return node;
    }

    bool has_node(const std::string& id) const { return index_.count(id) != 0; }

    void add_edge(const std::string& from, const std::string& to, const EdgeSpec& spec) {
        Edge e;
        e.from = from;
        e.to = to;
        e.label = spec.label;
        e.stroke = spec.stroke;
        e.start = spec.start;
        e.end = spec.end;
        e.length = spec.length;
        if (!spec.id.empty()) {
            e.id = spec.id;
        } else {
            int n = pair_count_[from + "\x1f" + to]++;
            e.id = "L_" + from + "_" + to + "_" + std::to_string(n);
        }
        model.edges.push_back(std::move(e));
    }

    bool has_edge_id(const std::string& id) const {
        return std::any_of(model.edges.begin(), model.edges.end(),
                           [&](const Edge& e) { return e.id == id; });
    }

    // Records membership of `id` in the innermost open subgraph.
    void note_member(const std::string& id) {
        if (stack_.empty()) return;
        Subgraph& sg = model.subgraphs[stack_.back()];
        if (std::find(sg.members.begin(), sg.members.end(), id) == sg.members.end())
            sg.members.push_back(id);
    }

    void open_subgraph(const std::string& id, const std::string& title, int line) {
        Subgraph sg;
        sg.id = id;
        sg.title = title;
        model.subgraphs.push_back(std::move(sg));
        note_member(id);  // registers in the parent (stack still points at it)
        stack_.push_back(model.subgraphs.size() - 1);
        open_lines_.push_back(line);
    }

    bool close_subgraph() {
        if (stack_.empty()) return false;
        stack_.pop_back();
        open_lines_.pop_back();
        return true;
    }

    bool in_subgraph() const { return !stack_.empty(); }
    Subgraph& current_subgraph() { return model.subgraphs[stack_.back()]; }
    int unclosed_line() const { return open_lines_.empty() ? 0 : open_lines_.back(); }

private:
    std::map<std::string, size_t> index_;
    int vertex_counter_ = 0;
    std::map<std::string, int> pair_count_;
    std::vector<size_t> stack_;
    std::vector<int> open_lines_;
};

class ChainParser {
public:
    ChainParser(const std::string& text, int line, Builder& b) : s_(text), line_(line), b_(b) {}

    void parse() {
        skip_ws();
        std::vector<std::string> prev = parse_group();
        while (true) {
            skip_ws();
            if (i_ >= s_.size()) return;
            EdgeSpec spec = parse_edge();
            skip_ws();
            if (i_ >= s_.size()) fail("expected a node after the edge");
            std::vector<std::string> next = parse_group();
            for (const std::string& from : prev)
                for (const std::string& to : next) b_.add_edge(from, to, spec);
            prev = std::move(next);
        }
    }

private:
    const std::string& s_;
    size_t i_ = 0;
    int line_;
    Builder& b_;

    [[noreturn]] void fail(const std::string& msg) const { throw ParseError(msg, line_); }
    bool at_end() const { return i_ >= s_.size(); }
    char peek(size_t off = 0) const { return i_ + off < s_.size() ? s_[i_ + off] : '\0'; }
    void skip_ws() {
        while (i_ < s_.size() && is_space(s_[i_])) ++i_;
    }

    std::vector<std::string> parse_group() {
        std::vector<std::string> ids;
        ids.push_back(parse_vertex());
        while (true) {
            size_t save = i_;
            skip_ws();
            if (peek() == '&') {
                ++i_;
                skip_ws();
                ids.push_back(parse_vertex());
            } else {
                i_ = save;
                break;
            }
        }
        return ids;
    }

    std::string read_id() {
        size_t start = i_;
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (is_id_char(c)) {
                ++i_;
            } else if (c == '-' && i_ > start && is_id_char(s_[i_ - 1]) && i_ + 1 < s_.size() &&
                       is_id_char(s_[i_ + 1])) {
                ++i_;  // `A-B` is one identifier, `A-->B` is not
            } else {
                break;
            }
        }
        return s_.substr(start, i_ - start);
    }

    // Finds the end of the label starting at `i_`; supports one quoted label.
    // Returns the raw label text and advances past the closing token.
    struct Close {
        const char* token;
        Shape shape;
    };
    std::string read_label(const std::vector<Close>& closers, Shape& shape) {
        size_t j = i_;
        while (j < s_.size() && s_[j] == ' ') ++j;
        size_t search_from = i_;
        if (j < s_.size() && s_[j] == '"') {
            size_t q = s_.find('"', j + 1);
            if (q == std::string::npos) fail("unterminated string in label");
            search_from = q + 1;
        }
        size_t best = std::string::npos;
        const Close* chosen = nullptr;
        for (const Close& c : closers) {
            size_t p = s_.find(c.token, search_from);
            if (p != std::string::npos && (best == std::string::npos || p < best)) {
                best = p;
                chosen = &c;
            }
        }
        if (best == std::string::npos) fail("unterminated node label");
        std::string text = s_.substr(i_, best - i_);
        i_ = best + std::char_traits<char>::length(chosen->token);
        shape = chosen->shape;
        return text;
    }

    void parse_data_block(Node& node) {
        // at '@{'
        i_ += 2;
        std::string body;
        bool in_quote = false;
        while (i_ < s_.size() && (in_quote || s_[i_] != '}')) {
            if (s_[i_] == '"') in_quote = !in_quote;
            body += s_[i_++];
        }
        if (i_ >= s_.size()) fail("unterminated '@{' block");
        ++i_;  // '}'
        std::string piece;
        in_quote = false;
        auto apply = [&](const std::string& item) {
            size_t colon = item.find(':');
            if (colon == std::string::npos) return;
            std::string key = lower(trim(item.substr(0, colon)));
            std::string value = trim(item.substr(colon + 1));
            if (key == "label") node.label = clean_label(value);
            else if (key == "shape") {
                if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                    value = value.substr(1, value.size() - 2);
                node.shape = shape_from_data_name(value);
            }
        };
        for (char c : body) {
            if (c == '"') in_quote = !in_quote;
            if (c == ',' && !in_quote) {
                apply(piece);
                piece.clear();
            } else {
                piece += c;
            }
        }
        apply(piece);
    }

    std::string parse_vertex() {
        skip_ws();
        std::string id = read_id();
        if (id.empty()) fail(std::string("unexpected '") + peek() + "'");

        // `e1@{curve: basis}` configures an edge, it is not a node.
        if (peek() == '@' && peek(1) == '{' && !b_.has_node(id) && b_.has_edge_id(id)) {
            Node scratch;
            parse_data_block(scratch);
            return id;
        }

        Node& node = b_.mention_node(id);
        if (peek() == '@' && peek(1) == '{') parse_data_block(node);

        static const std::vector<std::pair<const char*, std::vector<Close>>> shapes = {
            {"(((", {{")))", Shape::DoubleCircle}}},
            {"((", {{"))", Shape::Circle}}},
            {"([", {{"])", Shape::Stadium}}},
            {"[[", {{"]]", Shape::Subroutine}}},
            {"[(", {{")]", Shape::Cylinder}}},
            {"{{", {{"}}", Shape::Hexagon}}},
            {"[/", {{"/]", Shape::LeanRight}, {"\\]", Shape::Trapezoid}}},
            {"[\\", {{"\\]", Shape::LeanLeft}, {"/]", Shape::InvTrapezoid}}},
            {">", {{"]", Shape::Odd}}},
            {"(", {{")", Shape::Round}}},
            {"[", {{"]", Shape::Square}}},
            {"{", {{"}", Shape::Diamond}}},
        };
        for (const auto& [open, closers] : shapes) {
            size_t n = std::char_traits<char>::length(open);
            if (s_.compare(i_, n, open) == 0) {
                i_ += n;
                Shape shape = Shape::Default;
                std::string text = read_label(closers, shape);
                Node& again = b_.upsert_node(id);
                again.label = clean_label(text);
                again.shape = shape;
                break;
            }
        }

        if (s_.compare(i_, 3, ":::") == 0) {
            i_ += 3;
            size_t start = i_;
            while (i_ < s_.size() && (is_id_char(s_[i_]) || s_[i_] == '-')) ++i_;
            std::string cls = s_.substr(start, i_ - start);
            if (cls.empty()) fail("expected a class name after ':::'");
            Node& again = b_.upsert_node(id);
            if (std::find(again.classes.begin(), again.classes.end(), cls) == again.classes.end())
                again.classes.push_back(cls);
        }
        b_.note_member(id);
        return id;
    }

    std::string read_pipe_label() {
        size_t save = i_;
        while (i_ < s_.size() && s_[i_] == ' ') ++i_;
        if (peek() != '|') {
            i_ = save;
            return {};
        }
        ++i_;
        size_t j = i_;
        while (j < s_.size() && s_[j] == ' ') ++j;
        size_t close;
        if (j < s_.size() && s_[j] == '"') {
            size_t q = s_.find('"', j + 1);
            if (q == std::string::npos) fail("unterminated string in edge label");
            close = s_.find('|', q + 1);
        } else {
            close = s_.find('|', i_);
        }
        if (close == std::string::npos) fail("unterminated edge label '|...|'");
        std::string text = s_.substr(i_, close - i_);
        i_ = close + 1;
        return clean_label(text);
    }

    Head head_for(char c) const {
        switch (c) {
            case '>': return Head::Arrow;
            case 'x': return Head::Cross;
            case 'o': return Head::Circle;
            default: return Head::None;
        }
    }

    EdgeSpec parse_edge() {
        EdgeSpec spec;
        // optional `id@` prefix
        {
            size_t j = i_;
            while (j < s_.size() && (is_id_char(s_[j]) || s_[j] == '-')) ++j;
            if (j > i_ && j < s_.size() && s_[j] == '@' && j + 1 < s_.size() && s_[j + 1] != '{') {
                spec.id = s_.substr(i_, j - i_);
                i_ = j + 1;
            }
        }
        // optional start head
        if (peek() == '<') {
            spec.start = Head::Arrow;
            ++i_;
        } else if ((peek() == 'x' || peek() == 'o') && (peek(1) == '-' || peek(1) == '=')) {
            spec.start = head_for(peek());
            ++i_;
        }

        const char c = peek();
        if (c == '~') {
            size_t n = 0;
            while (peek(n) == '~') ++n;
            if (n < 3) fail("invalid link '~~'");
            i_ += n;
            spec.stroke = Stroke::Invisible;
            spec.end = Head::None;
            spec.length = static_cast<int>(n) - 2;
            spec.label = read_pipe_label();
            return spec;
        }
        if (c != '-' && c != '=') fail(std::string("unexpected '") + (c ? c : ' ') + "', expected a link");

        // dotted: -.-  -.->  -..->  and  -. text .->
        if (c == '-' && peek(1) == '.') {
            size_t j = i_ + 1;
            size_t dots = 0;
            while (j < s_.size() && s_[j] == '.') { ++dots; ++j; }
            spec.stroke = Stroke::Dotted;
            if (j < s_.size() && s_[j] == '-') {
                ++j;
                spec.end = Head::None;
                if (j < s_.size() && (s_[j] == '>' || s_[j] == 'x' || s_[j] == 'o')) {
                    spec.end = head_for(s_[j]);
                    ++j;
                }
                i_ = j;
                spec.length = static_cast<int>(dots);
                spec.label = read_pipe_label();
                return spec;
            }
            // text link: -. text .->
            size_t k = j;
            while (true) {
                k = s_.find(".-", k);
                if (k == std::string::npos) fail("unterminated dotted link text");
                break;
            }
            spec.label = clean_label(s_.substr(j, k - j));
            k += 2;
            spec.end = Head::None;
            if (k < s_.size() && (s_[k] == '>' || s_[k] == 'x' || s_[k] == 'o')) {
                spec.end = head_for(s_[k]);
                ++k;
            }
            i_ = k;
            return spec;
        }

        spec.stroke = c == '=' ? Stroke::Thick : Stroke::Normal;
        size_t n = 0;
        while (peek(n) == c) ++n;
        if (n < 2) fail(std::string("invalid link '") + c + "'");
        const char t = peek(n);
        if (t == '>' || t == 'x' || t == 'o') {
            spec.end = head_for(t);
            spec.length = static_cast<int>(n) - 1;
            i_ += n + 1;
        } else if (n >= 3) {
            spec.end = Head::None;
            spec.length = static_cast<int>(n) - 2;
            i_ += n;
        } else if (is_space(t)) {
            // text link: -- text -->
            size_t text_start = i_ + n;
            size_t k = text_start;
            while (true) {
                k = s_.find(std::string(2, c), k);
                if (k == std::string::npos) fail("unterminated link text");
                size_t m = 0;
                while (k + m < s_.size() && s_[k + m] == c) ++m;
                char e = k + m < s_.size() ? s_[k + m] : '\0';
                if (e == '>' || e == 'x' || e == 'o') {
                    spec.end = head_for(e);
                    spec.length = static_cast<int>(m) - 1;
                    spec.label = clean_label(s_.substr(text_start, k - text_start));
                    i_ = k + m + 1;
                    break;
                }
                if (m >= 3) {
                    spec.end = Head::None;
                    spec.length = static_cast<int>(m) - 2;
                    spec.label = clean_label(s_.substr(text_start, k - text_start));
                    i_ = k + m;
                    break;
                }
                k += m;
            }
            return spec;
        } else {
            fail("invalid link");
        }
        spec.label = read_pipe_label();
        return spec;
    }
};

std::vector<std::string> split_commas(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            std::string t = trim(cur);
            if (!t.empty()) out.push_back(t);
            cur.clear();
        } else {
            cur += c;
        }
    }
    std::string t = trim(cur);
    if (!t.empty()) out.push_back(t);
    return out;
}

void parse_subgraph_header(const std::string& rest, int line, Builder& b) {
    if (rest.empty()) throw ParseError("subgraph needs a name", line);
    std::string id, title;
    if (rest.front() == '"') {
        size_t q = rest.find('"', 1);
        if (q == std::string::npos) throw ParseError("unterminated subgraph title", line);
        title = rest.substr(1, q - 1);
        id = title;
    } else {
        size_t br = rest.find('[');
        if (br != std::string::npos) {
            id = trim(rest.substr(0, br));
            size_t close = rest.rfind(']');
            if (close == std::string::npos || close < br) throw ParseError("unterminated subgraph title", line);
            title = clean_label(rest.substr(br + 1, close - br - 1));
            if (id.empty()) id = title;
        } else {
            id = rest;
            title = rest;
        }
    }
    b.open_subgraph(id, decode_label(title), line);
}

}  // namespace

Model Parser::parse(std::string_view source) const {
    std::vector<Statement> stmts = detail::split_statements(source);
    if (stmts.empty()) throw ParseError("empty diagram", 1);

    Builder b;
    struct LinkStyle {
        std::vector<int> indices;
        bool all = false;
        Style style;
    };
    std::vector<LinkStyle> link_styles;

    // header
    {
        const Statement& h = stmts.front();
        std::string kw;
        size_t pos = 0;
        while (pos < h.text.size() && !is_space(h.text[pos])) ++pos;
        kw = h.text.substr(0, pos);
        if (kw != "graph" && kw != "flowchart" && kw != "flowchart-elk")
            throw ParseError("expected 'graph' or 'flowchart', found '" + kw + "'", h.line);
        std::string rest = trim(h.text.substr(pos));
        if (!rest.empty()) {
            size_t sp = 0;
            while (sp < rest.size() && !is_space(rest[sp])) ++sp;
            if (auto d = parse_direction(rest.substr(0, sp))) {
                b.model.direction = *d;
                rest = trim(rest.substr(sp));
            }
        }
        if (!rest.empty()) stmts.insert(stmts.begin() + 1, Statement{rest, h.line});
    }

    for (size_t k = 1; k < stmts.size(); ++k) {
        const std::string& t = stmts[k].text;
        const int line = stmts[k].line;

        if (starts_with_word(t, "subgraph")) {
            parse_subgraph_header(rest_after(t, "subgraph"), line, b);
        } else if (t == "end") {
            if (!b.close_subgraph()) throw ParseError("'end' without a matching 'subgraph'", line);
        } else if (starts_with_word(t, "direction")) {
            std::string d = rest_after(t, "direction");
            auto dir = parse_direction(d);
            if (!dir) throw ParseError("invalid direction '" + d + "'", line);
            if (b.in_subgraph()) b.current_subgraph().direction = dir;
        } else if (starts_with_word(t, "classDef")) {
            std::string rest = rest_after(t, "classDef");
            size_t sp = 0;
            while (sp < rest.size() && !is_space(rest[sp])) ++sp;
            if (sp == 0 || sp >= rest.size()) throw ParseError("classDef needs a name and a style", line);
            Style style = parse_style(rest.substr(sp));
            for (const std::string& name : split_commas(rest.substr(0, sp))) {
                if (!b.model.class_defs.count(name)) b.model.class_def_order.push_back(name);
                ClassDef& cd = b.model.class_defs[name];
                cd.name = name;
                cd.style = style;
            }
        } else if (starts_with_word(t, "class")) {
            std::string rest = rest_after(t, "class");
            size_t sp = rest.find_last_of(" \t");
            if (sp == std::string::npos) throw ParseError("class needs node ids and a class name", line);
            std::string cls = trim(rest.substr(sp));
            for (const std::string& id : split_commas(rest.substr(0, sp))) {
                Node& n = b.upsert_node(id);
                if (std::find(n.classes.begin(), n.classes.end(), cls) == n.classes.end())
                    n.classes.push_back(cls);
            }
        } else if (starts_with_word(t, "style")) {
            std::string rest = rest_after(t, "style");
            size_t sp = 0;
            while (sp < rest.size() && !is_space(rest[sp])) ++sp;
            if (sp == 0 || sp >= rest.size()) throw ParseError("style needs a node id and declarations", line);
            Node& n = b.mention_node(rest.substr(0, sp));
            Style st = parse_style(rest.substr(sp));
            n.style.insert(n.style.end(), st.begin(), st.end());
        } else if (starts_with_word(t, "linkStyle")) {
            std::string rest = rest_after(t, "linkStyle");
            size_t sp = 0;
            while (sp < rest.size() && !is_space(rest[sp])) ++sp;
            if (sp == 0 || sp >= rest.size()) throw ParseError("linkStyle needs indices and declarations", line);
            LinkStyle ls;
            std::string which = rest.substr(0, sp);
            if (which == "default") ls.all = true;
            else
                for (const std::string& part : split_commas(which)) {
                    char* end = nullptr;
                    long v = std::strtol(part.c_str(), &end, 10);
                    if (end == part.c_str() || *end != '\0' || v < 0)
                        throw ParseError("invalid linkStyle index '" + part + "'", line);
                    ls.indices.push_back(static_cast<int>(v));
                }
            ls.style = parse_style(rest.substr(sp));
            link_styles.push_back(std::move(ls));
        } else if (starts_with_word(t, "click") || starts_with_word(t, "callback") ||
                   starts_with_word(t, "href")) {
            // interactions are not rendered
        } else if (t.compare(0, 9, "accTitle:") == 0) {
            b.model.acc_title = trim(t.substr(9));
        } else if (t.compare(0, 9, "accDescr:") == 0) {
            b.model.acc_descr = trim(t.substr(9));
        } else {
            ChainParser(t, line, b).parse();
        }
    }

    if (b.in_subgraph()) throw ParseError("subgraph is never closed with 'end'", b.unclosed_line());

    // flowDb.updateLink: `linkStyle default` is the base style of every edge; an indexed
    // linkStyle replaces the edge's own style (the last one wins) and adds `fill:none` unless the
    // style sets a fill.
    Style default_style;
    std::vector<Style> own(b.model.edges.size());
    for (const LinkStyle& ls : link_styles) {
        if (ls.all) default_style = ls.style;
        for (int idx : ls.indices) {
            if (idx < 0 || static_cast<size_t>(idx) >= own.size()) continue;
            own[idx] = ls.style;
            const bool has_fill = std::any_of(ls.style.begin(), ls.style.end(), [](const auto& d) { return d.first.compare(0, 4, "fill") == 0; });
            if (!ls.style.empty() && !has_fill) own[idx].emplace_back("fill", "none");
        }
    }
    for (size_t i = 0; i < own.size(); ++i) {
        Edge& e = b.model.edges[i];
        e.style = default_style;
        e.style.insert(e.style.end(), own[i].begin(), own[i].end());
    }
    return std::move(b.model);
}

}  // namespace mermaid::flowchart

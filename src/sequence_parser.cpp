#include <mermaid/sequence_parser.h>

#include "init_directive.h"
#include "json.h"

#include <algorithm>
#include <cctype>
#include <mermaid/css_color.h>
#include <mermaid/error.h>

namespace mermaid::sequence {

namespace {

std::string trim(std::string_view s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return std::string(s.substr(b, e - b));
}

bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

bool is_word_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// `word` leads `text` as a whole word, ignoring case; `rest` receives what follows it.
bool take_keyword(std::string_view text, std::string_view word, std::string& rest) {
    if (text.size() < word.size() || !iequals(text.substr(0, word.size()), word)) return false;
    if (text.size() > word.size() && is_word_char(text[word.size()])) return false;
    rest = trim(text.substr(word.size()));
    return true;
}

// sequenceDb parseMessage: a leading `wrap:` or `nowrap:` (after an optional `:`) is a flag, not text.
struct parsed_text {
    std::string text;
    std::optional<bool> wrap;
};

parsed_text parse_message(std::string_view raw) {
    const std::string trimmed = trim(raw);
    std::string_view s = trimmed;
    parsed_text out;
    // the token's own colon is gone and the rest trimmed (`A->>B: wrap: text`), then the flag is looked for
    const std::string unprefixed = trim(s.empty() || s.front() != ':' ? s : s.substr(1));
    std::string_view body = unprefixed;
    if (body.substr(0, 5) == "wrap:") {
        out.wrap = true;
        out.text = trim(body.substr(5));
    } else if (body.substr(0, 7) == "nowrap:") {
        out.wrap = false;
        out.text = trim(body.substr(7));
    } else {
        out.text = trim(s.empty() || s.front() != ':' ? s : s.substr(1));
        // Mermaid strips the pattern `^:?(?:no)?wrap:` only; a plain leading colon belongs to the token, not the text
    }
    return out;
}

struct arrow {
    const char* token;
    LineType type;
};

// Longest first, so that `-->>` is not read as `->>` further along.
constexpr arrow kArrows[] = {{"<<-->>", LineType::BidirectionalDotted}, {"<<->>", LineType::BidirectionalSolid},
                             {"-->>", LineType::Dotted},               {"->>", LineType::Solid},
                             {"--x", LineType::DottedCross},           {"-x", LineType::SolidCross},
                             {"--)", LineType::DottedPoint},           {"-)", LineType::SolidPoint},
                             {"-->", LineType::DottedOpen},            {"->", LineType::SolidOpen},
                             {"--|\\", LineType::SolidTopDotted},             {"--|/", LineType::SolidBottomDotted},
                             {"--\\\\", LineType::StickTopDotted},            {"--//", LineType::StickBottomDotted},
                             {"/|--", LineType::SolidArrowTopReverseDotted}, {"\\|--", LineType::SolidArrowBottomReverseDotted},
                             {"//--", LineType::StickArrowTopReverseDotted}, {"\\\\--", LineType::StickArrowBottomReverseDotted},
                             {"-|\\", LineType::SolidTop},                    {"-|/", LineType::SolidBottom},
                             {"-\\\\", LineType::StickTop},                   {"-//", LineType::StickBottom},
                             {"/|-", LineType::SolidArrowTopReverse},        {"\\|-", LineType::SolidArrowBottomReverse},
                             {"//-", LineType::StickArrowTopReverse},        {"\\\\-", LineType::StickArrowBottomReverse}};


// CSS.supports('color', value) for what `box` accepts: keywords, hex, rgb(), hsl() and their alpha forms
bool is_css_color(const std::string& value) {
    if (iequals(value, "transparent") || parse_css_color(value)) return true;
    const size_t paren = value.find('(');
    const size_t last_comma = value.rfind(',');
    if (paren == std::string::npos || last_comma == std::string::npos || value.back() != ')') return false;
    const std::string name = value.substr(0, paren);
    if (!iequals(name, "rgba") && !iequals(name, "hsla")) return false;
    const std::string alpha = trim(std::string_view(value).substr(last_comma + 1, value.size() - last_comma - 2));
    if (alpha.empty() || alpha.find_first_not_of("0123456789.%") != std::string::npos) return false;
    return parse_css_color(name.substr(0, 3) + value.substr(paren, last_comma - paren) + ")").has_value();
}

// sanitizeText runs a box title through DOMPurify, whose serialisation writes every line break as `<br>`
std::string purify_breaks(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size();) {
        const size_t close = text.find('>', i);
        if (text.size() >= i + 3 && iequals(text.substr(i, 3), "<br") && close != std::string::npos &&
            trim(std::string_view(text).substr(i + 3, close - i - 3)).find_first_not_of("/") == std::string::npos) {
            out += "<br>";
            i = close + 1;
        } else {
            out += text[i++];
        }
    }
    return out;
}

// encodeEntities: `#35;` and `#infin;` become placeholders before the text is cut at `#` and `;`; the renderer decodes them.
std::string encode_entities(std::string_view text) {
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '#') {
            size_t j = i + 1;
            while (j < text.size() && (std::isalnum(static_cast<unsigned char>(text[j])) || text[j] == '_')) ++j;
            if (j > i + 1 && j < text.size() && text[j] == ';') {
                const std::string_view inner = text.substr(i + 1, j - i - 1);
                const bool numeric = inner.find_first_not_of("0123456789") == std::string_view::npos;
                out += numeric ? "\xEF\xAC\x82\xC2\xB0\xC2\xB0" : "\xEF\xAC\x82\xC2\xB0";  // ﬂ°° / ﬂ°
                out += inner;
                out += "\xC2\xB6\xC3\x9F";  // ¶ß
                i = j;
                continue;
            }
        }
        out += text[i];
    }
    return out;
}

class Reader {
public:
    Model run(std::string_view source) {
        model_.config = detail::parse_sequence_init(source);
        const std::string encoded = encode_entities(source);
        source = encoded;
        size_t line_no = 0;
        bool header = false;
        bool in_descr = false;
        std::string descr;
        size_t pos = 0;
        while (pos <= source.size()) {
            const size_t nl = source.find('\n', pos);
            std::string line(source.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos));
            pos = nl == std::string_view::npos ? source.size() + 1 : nl + 1;
            ++line_no;
            line_ = static_cast<int>(line_no);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (in_descr) {  // accDescr { ... }
                const size_t close = line.find('}');
                if (close == std::string::npos) {
                    descr += (descr.empty() ? "" : "\n") + trim(line);
                    continue;
                }
                const std::string last = trim(line.substr(0, close));
                if (!last.empty()) descr += (descr.empty() ? "" : "\n") + last;
                model_.acc_descr = descr;
                in_descr = false;
                continue;
            }
            for (std::string& statement : statements(line)) {
                if (statement.empty()) continue;
                if (!header) {
                    std::string rest;
                    if (!take_keyword(statement, "sequenceDiagram", rest)) fail("a sequence diagram starts with `sequenceDiagram`");
                    header = true;
                    if (!rest.empty()) statement = rest; else continue;
                }
                std::string rest;
                if (take_keyword(statement, "accDescr", rest) && rest.rfind('{', 0) == 0) {
                    in_descr = true;
                    descr.clear();
                    const std::string first = trim(rest.substr(1));
                    const size_t close = first.find('}');
                    if (close != std::string::npos) {
                        model_.acc_descr = trim(first.substr(0, close));
                        in_descr = false;
                    } else if (!first.empty()) {
                        descr = first;
                    }
                    continue;
                }
                statement_(statement);
            }
        }
        if (!header) fail("a sequence diagram starts with `sequenceDiagram`");
        if (!blocks_.empty()) fail("`end` is missing");
        return std::move(model_);
    }

private:
    [[noreturn]] void fail(const std::string& what) const { throw ParseError(what, line_); }

    // Comments and the statement separator: `#` and `%%` comment to the end of the line, `;` ends a statement.
    static std::vector<std::string> statements(const std::string& line) {
        std::string text = line;
        const std::string head = trim(text);
        if (head.rfind('#', 0) == 0 || head.rfind("%%", 0) == 0) return {};
        const size_t trailing = text.find("%%");
        if (trailing != std::string::npos) text.erase(trailing);
        std::vector<std::string> out;
        std::string current;
        for (char c : text) {
            if (c == '#') break;
            if (c == ';') {
                out.push_back(trim(current));
                current.clear();
            } else {
                current += c;
            }
        }
        out.push_back(trim(current));
        return out;
    }

    void statement_(const std::string& s) {
        std::string rest;
        if (take_keyword(s, "participant", rest)) return declare(rest, "participant");
        if (take_keyword(s, "actor", rest)) return declare(rest, "actor");
        if (take_keyword(s, "create", rest)) return create(rest);
        if (take_keyword(s, "destroy", rest)) {
            model_.destroyed_actors[actor_id(rest)] = model_.messages.size();
            return;
        }
        if (take_keyword(s, "link", rest)) return actor_link(rest);
        if (take_keyword(s, "links", rest)) return actor_json(rest, &Actor::links);
        if (take_keyword(s, "properties", rest)) return actor_json(rest, &Actor::properties);
        if (take_keyword(s, "details", rest)) return;  // Mermaid 12.2 reads the line and does nothing with it
        if (take_keyword(s, "box", rest)) return open_box(rest);
        if (take_keyword(s, "autonumber", rest)) return autonumber(rest);
        if (take_keyword(s, "activate", rest)) return activation(rest, LineType::ActiveStart);
        if (take_keyword(s, "deactivate", rest)) return activation(rest, LineType::ActiveEnd);
        if (take_keyword(s, "note", rest)) return note(rest);
        if (s.size() > 5 && iequals(std::string_view(s).substr(0, 5), "title") && (s[5] == ':' || std::isspace(static_cast<unsigned char>(s[5])))) {
            model_.title = trim(std::string_view(s).substr(s[5] == ':' ? 6 : 5));
            return;
        }
        if (s.size() > 8 && iequals(std::string_view(s).substr(0, 8), "accTitle")) return acc(s.substr(8), model_.acc_title);
        if (s.size() > 8 && iequals(std::string_view(s).substr(0, 8), "accDescr")) return acc(s.substr(8), model_.acc_descr);
        if (fragment(s)) return;
        message(s);
    }

    void acc(const std::string& after, std::string& target) {
        const size_t colon = after.find(':');
        if (colon == std::string::npos) fail("expected `:` after the accessibility keyword");
        target = trim(std::string_view(after).substr(colon + 1));
    }

    std::string actor_id(const std::string& text) const {
        const std::string id = trim(text);
        if (id.empty()) fail("an actor needs a name");
        return id;
    }

    Actor& ensure_actor(const std::string& key) {
        for (Actor& a : model_.actors)
            if (a.key == key) return a;
        Actor a;
        a.key = key;
        a.description = key;
        if (!boxes_.empty()) {
            a.box = static_cast<int>(boxes_.back());
            model_.boxes[boxes_.back()].actor_keys.push_back(key);
        }
        model_.actors.push_back(std::move(a));
        return model_.actors.back();
    }

    static void set_pair(std::vector<std::pair<std::string, std::string>>& list, const std::string& key, const std::string& value) {
        for (auto& [k, v] : list)
            if (k == key) {
                v = value;
                return;
            }
        list.emplace_back(key, value);
    }

    // `link A: label @ url`: addALink cuts the text at the first `@`, the label ends one character before it
    void actor_link(const std::string& rest) {
        const size_t colon = rest.find(':');
        if (colon == std::string::npos) return;
        Actor* actor = model_.find_actor(trim(std::string_view(rest).substr(0, colon)));
        if (!actor) return;
        const std::string text = trim(std::string_view(rest).substr(colon + 1));
        const size_t at = text.find('@');
        if (at == std::string::npos) return;
        const std::string label = trim(std::string_view(text).substr(0, at == 0 ? text.size() - 1 : at - 1));
        set_pair(actor->links, label, trim(std::string_view(text).substr(at + 1)));
    }

    // `links A: {"label": "url"}` and `properties A: {"class": "x"}`: a JSON object merged into the actor (bad JSON is ignored)
    void actor_json(const std::string& rest, std::vector<std::pair<std::string, std::string>> Actor::*field) {
        const size_t colon = rest.find(':');
        if (colon == std::string::npos) return;
        Actor* actor = model_.find_actor(trim(std::string_view(rest).substr(0, colon)));
        if (!actor) return;
        const auto object = detail::json_reader(std::string_view(rest).substr(colon + 1)).read_object();
        if (!object) return;
        for (const std::string& key : object->order) {
            const detail::json_value* value = object->find(key);
            if (value && value->type == detail::json_value::kind::string) set_pair(actor->*field, key, value->string);
        }
    }

    // `@{ "type": "boundary", "alias": "Bob" }` after a participant id: the keys of the metadata (JSON, or YAML's `key: value` pairs)
    static std::vector<std::pair<std::string, std::string>> metadata_pairs(const std::string& content) {
        std::vector<std::pair<std::string, std::string>> pairs;
        std::string item;
        bool quoted = false;
        const auto flush = [&] {
            const size_t colon = item.find(':');
            if (colon != std::string::npos) {
                const auto clean = [](std::string v) {
                    v = trim(v);
                    if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'') && v.back() == v.front()) v = v.substr(1, v.size() - 2);
                    return v;
                };
                pairs.emplace_back(clean(item.substr(0, colon)), clean(item.substr(colon + 1)));
            }
            item.clear();
        };
        for (char c : content) {
            if (c == '"') quoted = !quoted;
            if ((c == ',' || c == '\n') && !quoted) flush();
            else item += c;
        }
        flush();
        return pairs;
    }

    void declare(const std::string& rest_in, const std::string& default_type) {
        std::string rest = rest_in, type = default_type, alias;
        if (const size_t at = rest.find("@{"); at != std::string::npos) {
            const size_t close = rest.find('}', at);
            if (close == std::string::npos) fail("the metadata of a participant is not closed");
            for (const auto& [key, value] : metadata_pairs(rest.substr(at + 2, close - at - 2))) {
                if (key == "type") type = value;
                else if (key == "alias") alias = value;
            }
            const std::string after = trim(std::string_view(rest).substr(close + 1));
            rest = trim(std::string_view(rest).substr(0, at)) + (after.empty() ? "" : " " + after);
        }
        std::string id = rest, description;
        bool has_description = false;
        // `participant A as Alice`: the id has no blanks when a description follows
        const size_t first_blank = rest.find_first_of(" \t");
        if (first_blank != std::string::npos) {
            const std::string after = trim(std::string_view(rest).substr(first_blank));
            std::string tail;
            if (take_keyword(after, "as", tail)) {
                id = rest.substr(0, first_blank);
                description = tail;
                has_description = true;
            }
        }
        id = actor_id(id);
        Actor& actor = ensure_actor(id);
        actor.type = type;
        if (!alias.empty() && !has_description) {  // an alias stands in for a missing description
            actor.description = alias;
            actor.wrap = model_.config.wrap.value_or(false);
        }
        if (has_description) {
            const parsed_text parsed = parse_message(description);
            actor.description = parsed.text;
            actor.wrap = parsed.wrap.value_or(model_.config.wrap.value_or(false));
        }
    }

    void create(const std::string& rest) {
        std::string tail;
        if (take_keyword(rest, "participant", tail)) declare(tail, "participant");
        else if (take_keyword(rest, "actor", tail)) declare(tail, "actor");
        else fail("`create` is followed by `participant` or `actor`");
        std::string id = tail;
        const size_t blank = tail.find_first_of(" \t@");
        if (blank != std::string::npos) id = tail.substr(0, blank);
        model_.created_actors[actor_id(id)] = model_.messages.size();
    }

    void open_box(const std::string& rest) {
        Box box;
        std::string text = rest;
        // `box rgb(200,220,255) Backend`: a leading CSS colour is the fill, the rest the title
        size_t end = 0;
        if (!text.empty() && text[0] != ' ') {
            const size_t paren = text.find('(');
            const size_t blank = text.find(' ');
            if (paren != std::string::npos && (blank == std::string::npos || paren < blank)) {
                const size_t close = text.find(')', paren);
                end = close == std::string::npos ? 0 : close + 1;
            } else {
                end = blank == std::string::npos ? text.size() : blank;
            }
        }
        const std::string color = text.substr(0, end);
        // parseBoxData: a leading colour is the fill, otherwise the whole line is the title over a transparent box
        if (!color.empty() && is_css_color(color)) {
            box.fill = color;
            text = trim(std::string_view(text).substr(end));
        } else {
            box.fill = "transparent";
        }
        const parsed_text parsed = parse_message(text);
        box.name = purify_breaks(parsed.text);
        box.wrap = parsed.wrap.value_or(model_.config.wrap.value_or(false));
        model_.boxes.push_back(std::move(box));
        boxes_.push_back(model_.boxes.size() - 1);
        blocks_.push_back(block::Box);
    }

    void autonumber(const std::string& rest) {
        Message m = fresh(LineType::Autonumber);
        if (iequals(rest, "off")) {
            m.number_visible = false;
        } else {
            m.number_visible = true;
            if (!rest.empty()) {
                const size_t blank = rest.find_first_of(" \t");
                const std::string first = rest.substr(0, blank), second = blank == std::string::npos ? "" : trim(std::string_view(rest).substr(blank));
                if (first.find_first_not_of("0123456789") != std::string::npos) fail("autonumber expects numbers");
                m.number_start = std::stoi(first);
                m.number_step = second.empty() ? 1 : std::stoi(second);
            }
        }
        model_.messages.push_back(std::move(m));
    }

    void activation(const std::string& rest, LineType type) {
        Message m = fresh(type);
        m.from = actor_id(rest);  // the actor is not registered here: Mermaid adds it with its first message
        model_.messages.push_back(std::move(m));
    }

    void note(const std::string& rest) {
        std::string tail;
        Placement placement;
        if (take_keyword(rest, "left of", tail)) placement = Placement::LeftOf;
        else if (take_keyword(rest, "right of", tail)) placement = Placement::RightOf;
        else if (take_keyword(rest, "over", tail)) placement = Placement::Over;
        else fail("a note is `left of`, `right of` or `over` an actor");
        const size_t colon = tail.find(':');
        if (colon == std::string::npos) fail("a note needs `:` and its text");
        const std::string who = tail.substr(0, colon);
        const parsed_text parsed = parse_message(std::string_view(tail).substr(colon));
        Message m = fresh(LineType::Note);
        const size_t comma = who.find(',');
        m.from = actor_id(who.substr(0, comma));
        m.to = comma == std::string::npos ? m.from : actor_id(who.substr(comma + 1));
        ensure_actor(m.from);
        ensure_actor(m.to);
        m.text = parsed.text;
        m.wrap = parsed.wrap.value_or(model_.config.wrap.value_or(false));
        m.placement = placement;
        model_.messages.push_back(std::move(m));
    }

    bool fragment(const std::string& s) {
        struct opener {
            const char* word;
            LineType start, end;
        };
        static constexpr opener kOpeners[] = {
            {"loop", LineType::LoopStart, LineType::LoopEnd},         {"opt", LineType::OptStart, LineType::OptEnd},
            {"alt", LineType::AltStart, LineType::AltEnd},            {"par_over", LineType::ParOverStart, LineType::ParEnd},
            {"par", LineType::ParStart, LineType::ParEnd},            {"critical", LineType::CriticalStart, LineType::CriticalEnd},
            {"break", LineType::BreakStart, LineType::BreakEnd},      {"rect", LineType::RectStart, LineType::RectEnd}};
        std::string rest;
        for (const opener& o : kOpeners) {
            if (!take_keyword(s, o.word, rest)) continue;
            add_text(o.start, rest);
            blocks_.push_back(block::Fragment);
            ends_.push_back(o.end);
            return true;
        }
        if (take_keyword(s, "else", rest)) return add_text(LineType::AltElse, rest), true;
        if (take_keyword(s, "and", rest)) return add_text(LineType::ParAnd, rest), true;
        if (take_keyword(s, "option", rest)) return add_text(LineType::CriticalOption, rest), true;
        if (take_keyword(s, "end", rest)) {
            if (blocks_.empty()) fail("`end` without an opening statement");
            const block closed = blocks_.back();
            blocks_.pop_back();
            if (closed == block::Box) {
                boxes_.pop_back();
            } else {
                add_text(ends_.back(), "");
                ends_.pop_back();
            }
            return true;
        }
        return false;
    }

    void add_text(LineType type, const std::string& raw) {
        const parsed_text parsed = parse_message(raw);
        Message m = fresh(type);
        m.text = parsed.text;
        m.wrap = parsed.wrap.value_or(model_.config.wrap.value_or(false));
        model_.messages.push_back(std::move(m));
    }

    void message(const std::string& s) {
        size_t best = std::string::npos;
        const arrow* found = nullptr;
        for (const arrow& a : kArrows) {
            const size_t at = s.find(a.token);
            if (at != std::string::npos && (best == std::string::npos || at < best)) {
                best = at;
                found = &a;
            }
        }
        if (!found || best == 0) fail("cannot read `" + s + "`");
        std::string before = trim(std::string_view(s).substr(0, best));
        std::string after = s.substr(best + std::char_traits<char>::length(found->token));
        // `A()->>B`, `A->>()B`, `A()->>()B`: a connection point at the start and/or the end of the line
        bool central_from = false, central_to = false;
        if (before.size() >= 2 && before.compare(before.size() - 2, 2, "()") == 0) {
            central_from = true;
            before.erase(before.size() - 2);
        }
        const size_t first = after.find_first_not_of(" \t");
        if (const std::string head = first == std::string::npos ? "" : after.substr(first); head.compare(0, 2, "()") == 0) {
            central_to = true;
            after = head.substr(2);
        }
        const std::string from = actor_id(before);
        bool activate = false, deactivate = false;
        if (central_to) activate = true;
        else if (!after.empty() && after[0] == '+') activate = true;
        else if (!after.empty() && after[0] == '-') deactivate = true;
        if ((activate || deactivate) && !central_to) after.erase(0, 1);
        const size_t colon = after.find(':');
        const std::string to = actor_id(after.substr(0, colon));
        const parsed_text parsed = colon == std::string::npos ? parsed_text{} : parse_message(std::string_view(after).substr(colon));
        ensure_actor(from);
        ensure_actor(to);
        Message m = fresh(found->type);
        m.from = from;
        m.to = to;
        m.text = parsed.text;
        m.wrap = parsed.wrap.value_or(model_.config.wrap.value_or(false));
        m.activate = activate;
        if (central_from || central_to)
            m.central = static_cast<int>(central_from && central_to ? LineType::CentralConnectionDual
                                         : central_to              ? LineType::CentralConnection
                                                                   : LineType::CentralConnectionReverse);
        model_.messages.push_back(std::move(m));
        if (central_to) {  // the end of the line starts an activation, the start of it too
            Message start = fresh(LineType::CentralConnection);
            start.from = to;
            model_.messages.push_back(std::move(start));
        }
        if (central_from) {
            Message start = fresh(LineType::CentralConnectionReverse);
            start.from = from;
            model_.messages.push_back(std::move(start));
        }
        if (central_to || central_from) {
        } else if (activate) {  // `->>+B` starts an activation of B, `-->>-B` ends one of the sender
            Message start = fresh(LineType::ActiveStart);
            start.from = to;
            model_.messages.push_back(std::move(start));
        } else if (deactivate) {
            Message end = fresh(LineType::ActiveEnd);
            end.from = from;
            model_.messages.push_back(std::move(end));
        }
    }

    Message fresh(LineType type) const {
        Message m;
        m.id = std::to_string(model_.messages.size());
        m.type = type;
        return m;
    }

    enum class block { Box, Fragment };

    Model model_;
    std::vector<block> blocks_;
    std::vector<LineType> ends_;
    std::vector<size_t> boxes_;
    int line_ = 0;
};

}  // namespace

Model Parser::parse(std::string_view source) const { return Reader().run(source); }

}  // namespace mermaid::sequence

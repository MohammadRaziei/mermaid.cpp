#include "markdown_label.h"

#include <algorithm>
#include <cctype>

#include <mermaid/macros.h>

namespace mermaid::detail {
namespace {

bool is_blank(char c) { return c == ' ' || c == '\t'; }
bool is_word_char(char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0; }

FontFace with(FontFace face, bool bold, bool italic) {
    const bool b = bold || face == FontFace::Bold || face == FontFace::BoldItalic;
    const bool i = italic || face == FontFace::Italic || face == FontFace::BoldItalic;
    return b ? (i ? FontFace::BoldItalic : FontFace::Bold) : (i ? FontFace::Italic : FontFace::Regular);
}

void escape_into(std::string& out, std::string_view text) {
    for (const char c : text) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else out += c;
    }
}

size_t delimiter_run(std::string_view s, size_t at) {
    size_t n = 0;
    while (at + n < s.size() && s[at + n] == s[at]) ++n;
    return n;
}

}  // namespace

// Inline emphasis, the part of CommonMark's rules these labels need: a delimiter run of 1 to 3 `*` or `_` opens when
// text that is not blank follows it (for `_` also when no word character precedes it), and closes at the next run of
// the same character that has text before it (for `_` no word character after it).
class markdown_inline {
public:
    markdown_inline(std::vector<markdown_label_run>& runs, std::string& html) : runs_(runs), html_(html) {}

    void parse(std::string_view s, FontFace face) {
        std::string text;
        const auto flush = [&] {
            if (text.empty()) return;
            runs_.push_back({text, face});
            escape_into(html_, text);
            text.clear();
        };
        for (size_t i = 0; i < s.size();) {
            const char c = s[i];
            if (c != '*' && c != '_') {
                text += s[i++];
                continue;
            }
            const size_t n = std::min<size_t>(delimiter_run(s, i), 3);
            const size_t after = i + delimiter_run(s, i);
            const bool opens = after < s.size() && !is_blank(s[after]) && (c == '*' || i == 0 || !is_word_char(s[i - 1]));
            const size_t close = opens && delimiter_run(s, i) == n ? find_closer(s, after, c, n) : std::string_view::npos;
            if (close == std::string_view::npos) {
                text.append(delimiter_run(s, i), c);
                i = after;
                continue;
            }
            flush();
            html_ += n == 1 ? "<em>" : n == 2 ? "<strong>" : "<em><strong>";
            parse(s.substr(after, close - after), with(face, n >= 2, n != 2));
            html_ += n == 1 ? "</em>" : n == 2 ? "</strong>" : "</strong></em>";
            i = close + n;
        }
        flush();
    }

private:
    static size_t find_closer(std::string_view s, size_t from, char c, size_t n) {
        for (size_t j = from; j < s.size(); ++j) {
            if (s[j] != c) continue;
            const size_t run = delimiter_run(s, j);
            const bool closes = run == n && j > from && !is_blank(s[j - 1]) && (c == '*' || j + n >= s.size() || !is_word_char(s[j + n]));
            if (closes) return j;
            j += run - 1;
        }
        return std::string_view::npos;
    }

    std::vector<markdown_label_run>& runs_;
    std::string& html_;
};

markdown_label::markdown_label(std::string_view text) {
    html_ = "<p>";
    bool first = true;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t nl = text.find('\n', start);
        std::string_view raw = text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        start = nl == std::string_view::npos ? text.size() + 1 : nl + 1;
        // the line as HTML sees it: blanks collapse, the ends are trimmed; an empty line is no line
        std::string collapsed;
        for (const char c : raw) {
            if (is_blank(c) && (collapsed.empty() || collapsed.back() == ' ')) continue;
            collapsed += is_blank(c) ? ' ' : c;
        }
        while (!collapsed.empty() && collapsed.back() == ' ') collapsed.pop_back();
        if (collapsed.empty()) continue;
        if (!first) html_ += "<br />";
        first = false;
        line runs;
        markdown_inline(runs, html_).parse(collapsed, FontFace::Regular);
        lines_.push_back(std::move(runs));
    }
    html_ += "</p>";
}

namespace {

using run_t = markdown_label_run;

double runs_width(const TextMeasurer& m, const std::vector<run_t>& runs) {
    double width = 0;
    for (size_t i = 0; i < runs.size();) {  // adjacent runs of one face are shaped together
        std::string joined = runs[i].text;
        size_t j = i + 1;
        while (j < runs.size() && runs[j].face == runs[i].face) joined += runs[j++].text;
        width += m.run_width(joined, runs[i].face);
        i = j;
    }
    return width;
}

struct word {
    std::vector<run_t> runs;
    bool blank_follows = false;
    FontFace space_after = FontFace::Regular;  // the face of the blank that follows it
};

std::vector<word> split_words(const std::vector<run_t>& runs) {
    std::vector<word> words(1);
    for (const run_t& r : runs) {
        std::string piece;
        for (const char c : r.text) {
            if (c == ' ') {
                if (!piece.empty()) words.back().runs.push_back({piece, r.face});
                piece.clear();
                words.back().space_after = r.face;
                words.back().blank_follows = true;
                words.emplace_back();
            } else {
                piece += c;
            }
        }
        if (!piece.empty()) words.back().runs.push_back({piece, r.face});
    }
    if (words.back().runs.empty()) words.pop_back();
    return words;
}

}  // namespace

std::vector<std::vector<TextWord>> markdown_label::words() const {
    std::vector<std::vector<TextWord>> out;
    for (const line& l : lines_) {
        out.emplace_back();
        for (const word& w : split_words(l))
            for (const run_t& r : w.runs) out.back().push_back({r.text, r.face});
    }
    return out;
}

TextMeasurer::Wrapped markdown_label::measure(const TextMeasurer& measurer, double max_width) const {
    TextMeasurer::Wrapped out;
    double widest = 0, table_width = max_width;
    int rows = 0;
    std::vector<std::vector<word>> split;
    for (const line& l : lines_) {
        split.push_back(split_words(l));
        for (const word& w : split.back()) {  // break-spaces keeps a blank at the end of its line: the word needs room for it
            std::vector<run_t> with_blank = w.runs;
            if (w.blank_follows) with_blank.push_back({" ", w.space_after});
            table_width = MERMAID_MAX(table_width, runs_width(measurer, with_blank));
        }
    }
    for (size_t k = 0; k < lines_.size(); ++k) {
        const double natural = runs_width(measurer, lines_[k]);
        if (natural <= max_width) {
            widest = MERMAID_MAX(widest, natural);
            ++rows;
            continue;
        }
        out.wrapped = true;
        std::vector<run_t> current;
        FontFace pending_space = FontFace::Regular;
        for (const word& w : split[k]) {
            std::vector<run_t> candidate = current;
            if (!candidate.empty()) candidate.push_back({" ", pending_space});
            candidate.insert(candidate.end(), w.runs.begin(), w.runs.end());
            if (current.empty() || runs_width(measurer, candidate) <= table_width) {
                current = candidate;
            } else {
                ++rows;
                current = w.runs;
            }
            pending_space = w.space_after;
        }
        ++rows;
    }
    out.size = {out.wrapped ? table_width : widest, rows * measurer.style().line_height()};
    return out;
}

}  // namespace mermaid::detail

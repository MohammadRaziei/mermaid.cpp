#include "svg_label.h"

#include <cmath>
#include <mermaid/macros.h>

#include "rough_path.h"  // js_number

namespace mermaid::detail {

namespace {

constexpr double kLineHeightEm = 1.1;  // createFormattedText

// getComputedTextLength of a row: the words in their faces, a blank leading every word after the first.
double row_width(const TextMeasurer& measurer, const svg_label::line& words) {
    double width = 0;
    for (size_t i = 0; i < words.size(); ++i) width += measurer.run_width((i ? " " : "") + words[i].text, words[i].face);
    return width;
}

// splitTextToChars: the code points of a UTF-8 word.
std::vector<std::string> code_points(const std::string& word) {
    std::vector<std::string> out;
    for (size_t i = 0; i < word.size();) {
        size_t n = 1;
        const unsigned char c = static_cast<unsigned char>(word[i]);
        if (c >= 0xF0) n = 4;
        else if (c >= 0xE0) n = 3;
        else if (c >= 0xC0) n = 2;
        out.push_back(word.substr(i, n));
        i += n;
    }
    return out;
}

// getBBox() of a <text> is wider than its advance by what the last glyph reaches beyond its own advance. Measured in
// Chromium with Liberation Sans at 14px; it depends on that glyph only (others reach 0). Scaled with the font size.
double last_glyph_overhang(char c, double font_size) {
    double px14 = 0;
    switch (c) {
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
        case '#': case '$': case '?': case 'L': case '_': case 'a': case 'b': case 'e': case 'o': case 'p': px14 = 0.204; break;
        case '"': px14 = 0.023; break;
        case '&': case 'A': case 'K': case 'V': case 'X': case 'Y': px14 = 0.653; break;
        case '(': case '-': case 'r': px14 = 0.330; break;
        case '*': px14 = 0.544; break;
        case '/': case 'O': case 'Q': case '[': case '\\': case 'f': case 't': px14 = 0.1065; break;
        case 'T': case 'Z': px14 = 0.440; break;
        case 'W': px14 = 0.778; break;
        case '^': px14 = 0.424; break;
        case 'k': px14 = 0.992; break;
        case 'w': px14 = 0.8825; break;
        case '{': case '}': px14 = 0.315; break;
        default: break;
    }
    return px14 * font_size / 14.0;
}

}  // namespace

std::string svg_label::row_y(size_t index) { return js_number(static_cast<double>(index) * kLineHeightEm - 0.1) + "em"; }

svg_label svg_label::layout(const TextMeasurer& measurer, std::string_view text, double max_width) {
    std::vector<line> paragraphs;
    size_t start = 0;
    for (;;) {
        const size_t nl = text.find('\n', start);
        const std::string_view paragraph = text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        line words;
        for (size_t pos = 0; pos <= paragraph.size();) {
            const size_t space = paragraph.find(' ', pos);
            std::string word(paragraph.substr(pos, space == std::string_view::npos ? std::string_view::npos : space - pos));
            pos = space == std::string_view::npos ? paragraph.size() + 1 : space + 1;
            if (!word.empty()) words.push_back({std::move(word), FontFace::Regular});
        }
        paragraphs.push_back(std::move(words));
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return layout_words(measurer, paragraphs, max_width);
}

svg_label svg_label::layout_words(const TextMeasurer& measurer, const std::vector<line>& paragraphs, double max_width) {
    const auto fits = [&](const line& words) { return row_width(measurer, words) <= max_width; };
    svg_label out;
    for (const line& words_in : paragraphs) {
        line words = words_in;
        if (fits(words)) {
            out.lines.push_back(words);
            continue;
        }
        // splitLineToFitWidth: greedy, a word wider than the label breaks between characters
        line current;
        for (size_t i = 0; i < words.size(); ++i) {
            line with = current;
            with.push_back(words[i]);
            if (fits(with)) {
                current = with;
                continue;
            }
            if (!current.empty()) {
                out.lines.push_back(current);
                current.clear();
                --i;  // the word starts the next row
                continue;
            }
            const std::vector<std::string> chars = code_points(words[i].text);
            std::string used;
            size_t taken = 0;
            while (taken < chars.size() && (taken == 0 || fits({{used + chars[taken], words[i].face}}))) used += chars[taken++];
            out.lines.push_back({{used, words[i].face}});
            std::string rest;
            for (size_t k = taken; k < chars.size(); ++k) rest += chars[k];
            if (!rest.empty()) words[i].text = rest, --i;
        }
        if (!current.empty()) out.lines.push_back(current);
    }
    // The glyph box of a row is its rounded ascent plus its rounded descent (16 at 14px); rows are 1.1em apart.
    const double em = measurer.style().font_size;
    const double ascent = std::round(0.905 * em), descent = std::round(0.212 * em);
    // The rows are centred on x = 0 (text-anchor: middle), so the box runs from half the widest row to the right-most
    // glyph edge of any row.
    double widest = 0;
    for (const line& l : out.lines) widest = MERMAID_MAX(widest, row_width(measurer, l));
    double right = widest / 2;
    for (const line& l : out.lines) {
        const double advance = row_width(measurer, l);
        const bool has_text = !l.empty() && !l.back().text.empty();
        right = MERMAID_MAX(right, advance / 2 + (has_text ? last_glyph_overhang(l.back().text.back(), em) : 0.0));
    }
    out.left = -widest / 2;
    const double rows = static_cast<double>(out.lines.size());
    out.size = {right + widest / 2, ascent + descent + (rows > 1 ? (rows - 1) * kLineHeightEm * em : 0.0)};
    out.top = em - ascent;
    return out;
}

}  // namespace mermaid::detail

namespace mermaid::detail {

void svg_label::write_text(SvgWriter& out, const std::vector<line>& rows, bool centered, bool style) {
    SvgWriter::Attrs text{{"y", "-10.1"}};
    if (centered) text.emplace_back("text-anchor", "middle");
    if (style) text.emplace_back("style", "");
    out.open("text", text);
    for (size_t r = 0; r < rows.size(); ++r) {
        SvgWriter::Attrs row{{"class", "text-outer-tspan row"}, {"x", "0"}, {"y", row_y(r)}, {"dy", "1.1em"}};
        if (centered) row.emplace_back("text-anchor", "middle");
        out.open("tspan", row);
        for (size_t w = 0; w < rows[r].size(); ++w)
        {
            const FontFace face = rows[r][w].face;
            const bool bold = face == FontFace::Bold || face == FontFace::BoldItalic;
            const bool italic = face == FontFace::Italic || face == FontFace::BoldItalic;
            out.element("tspan", {{"font-style", italic ? "italic" : "normal"}, {"class", "text-inner-tspan"},
                                  {"font-weight", bold ? "bold" : "normal"}},
                        (w ? " " : "") + rows[r][w].text);
        }
        out.close();
    }
    out.close();
}

}  // namespace mermaid::detail

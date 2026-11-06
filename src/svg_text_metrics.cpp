#include "svg_text_metrics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "svg_text_metrics_data.h"

namespace mermaid::detail {

namespace {

constexpr double kUnit = 256.0;
constexpr double kFallbackAdvance = 8.0;   // a character the table does not hold: half an em
constexpr double kWideAdvance = 16.0;      // CJK and other wide characters

std::vector<uint32_t> code_points(std::string_view s) {
    std::vector<uint32_t> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i++]);
        int extra = 0;
        uint32_t cp = c;
        if (c >= 0xF0) { extra = 3; cp = c & 0x07; }
        else if (c >= 0xE0) { extra = 2; cp = c & 0x0F; }
        else if (c >= 0xC0) { extra = 1; cp = c & 0x1F; }
        while (extra-- > 0 && i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i++]) & 0x3F);
        out.push_back(cp);
    }
    return out;
}

template <size_t N>
const svg_glyph* find_glyph(const svg_glyph (&table)[N], uint32_t cp) {
    const svg_glyph* end = table + N;
    const svg_glyph* it = std::lower_bound(table, end, cp, [](const svg_glyph& g, uint32_t v) { return g.code < v; });
    return it != end && it->code == cp ? it : nullptr;
}

double advance_of(uint32_t cp) {
    if (cp == ' ') return kSvgSpaceUnits / kUnit;
    if (const svg_glyph* g = find_glyph(kSerifAdvance, cp)) return g->units / kUnit;
    return cp >= 0x2E80 ? kWideAdvance : kFallbackAdvance;
}

double kerning_of(uint32_t a, uint32_t b) {
    if (a > 0xFF || b > 0xFF) return 0;
    const svg_pair* end = kSerifKerning + sizeof(kSerifKerning) / sizeof(kSerifKerning[0]);
    const svg_pair* it = std::lower_bound(kSerifKerning, end, std::pair<unsigned, unsigned>(a, b),
                                          [](const svg_pair& p, const std::pair<unsigned, unsigned>& key) {
                                              return std::pair<unsigned, unsigned>(p.first, p.second) < key;
                                          });
    return it != end && it->first == a && it->second == b ? it->units / kUnit : 0;
}

template <size_t N>
int tallest(const svg_glyph (&table)[N], const std::vector<uint32_t>& cps) {
    int height = 17;
    for (uint32_t cp : cps)
        if (const svg_glyph* g = find_glyph(table, cp)) height = std::max(height, g->units);
    return height;
}

}  // namespace

svg_text_dimensions svg_text_metrics::serif(std::string_view text) {
    // an SVG <text> collapses runs of blanks and drops the leading and trailing ones
    std::vector<uint32_t> cps;
    for (uint32_t cp : code_points(text)) {
        if (cp == '\t' || cp == '\n' || cp == '\r') cp = ' ';
        if (cp == ' ' && (cps.empty() || cps.back() == ' ')) continue;
        cps.push_back(cp);
    }
    while (!cps.empty() && cps.back() == ' ') cps.pop_back();
    if (cps.empty()) return {};
    double width = 0;
    for (size_t i = 0; i < cps.size(); ++i) {
        width += advance_of(cps[i]);
        if (i + 1 < cps.size()) width += kerning_of(cps[i], cps[i + 1]);
    }
    if (const svg_glyph* g = find_glyph(kSerifOverhang, cps.back())) width += g->units / kUnit;
    return {std::floor(0.5 + width), static_cast<double>(tallest(kSerifTall, cps))};
}

double svg_text_metrics::drawn_height(std::string_view text) {
    return static_cast<double>(tallest(kSansTall, code_points(text)));
}

std::vector<std::string> split_breaks(std::string_view text) {
    // common.lineBreakRegex: <br>, <br/>, <br />, </br>, in any case
    std::vector<std::string> rows;
    std::string current;
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '<') {
            size_t j = i + 1;
            if (j < text.size() && text[j] == '/') ++j;
            if (j + 1 < text.size() && (text[j] == 'b' || text[j] == 'B') && (text[j + 1] == 'r' || text[j + 1] == 'R')) {
                j += 2;
                while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j]))) ++j;
                if (j < text.size() && text[j] == '/') ++j;
                if (j < text.size() && text[j] == '>') {
                    rows.push_back(current);
                    current.clear();
                    i = j + 1;
                    continue;
                }
            }
        }
        current += text[i++];
    }
    rows.push_back(current);
    return rows;
}

svg_text_dimensions svg_text_metrics::calculate(std::string_view text) const {
    if (text.empty()) return {};  // calculateTextDimensions answers an empty text with nothing at all
    // every row is measured on its own: the width is the widest, the height the sum, the line height the tallest row
    // (an empty row is a zero-width space)
    svg_text_dimensions first, second;
    double first_line = 0, second_line = 0;
    for (const std::string& row : split_breaks(text)) {
        const svg_text_dimensions s = serif(row);
        const double serif_row = s.height > 0 ? s.height : 17;
        second.width = std::max(second.width, s.width);
        second.height += serif_row;
        second_line = std::max(second_line, serif_row);
        const double sans_row = static_cast<double>(tallest(kSansTall, code_points(row)));
        // the sans run: its advance (flowchart table) plus what the last glyph reaches beyond it at 16px (kSansOverhang)
        double sans_width = 0;
        if (!row.empty()) {
            const std::vector<uint32_t> cps = code_points(row);
            const svg_glyph* overhang = find_glyph(kSansOverhang, cps.back());
            sans_width = std::floor(0.5 + sans_.line_width(row) + (overhang ? overhang->units / kUnit : 0.0));
        }
        first.width = std::max(first.width, sans_width);
        first.height += sans_row;
        first_line = std::max(first_line, sans_row);
    }
    // the first family wins only when it is strictly bigger in all three
    const bool first_is_bigger = first.height > second.height && first.width > second.width && first_line > second_line;
    return first_is_bigger ? first : second;
}

}  // namespace mermaid::detail

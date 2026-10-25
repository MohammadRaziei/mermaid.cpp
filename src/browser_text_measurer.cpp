#include <mermaid/browser_text_measurer.h>

#include <algorithm>
#include <cstdint>
#include <string>

#include "browser_font_metrics.h"

namespace mermaid {
namespace {

using detail::font_advance;
using detail::font_kerning;

constexpr double kTableFontSize = 14.0;
constexpr double kTableUnit = 64.0;
constexpr double kNarrowFallbackEm = 0.556;  // other non-wide characters: an average Latin letter
constexpr double kWideFallbackEm = 1.0;      // CJK, emoji and other wide characters

// Decodes one UTF-8 sequence starting at `at`; invalid bytes decode as themselves.
uint32_t next_code_point(std::string_view s, size_t& at) {
    const unsigned char c = static_cast<unsigned char>(s[at++]);
    int extra = 0;
    uint32_t cp = c;
    if (c >= 0xF0) { extra = 3; cp = c & 0x07; }
    else if (c >= 0xE0) { extra = 2; cp = c & 0x0F; }
    else if (c >= 0xC0) { extra = 1; cp = c & 0x1F; }
    while (extra-- > 0 && at < s.size() && (static_cast<unsigned char>(s[at]) & 0xC0) == 0x80)
        cp = (cp << 6) | (static_cast<unsigned char>(s[at++]) & 0x3F);
    return cp;
}

// Advance in table units, or a negative number when the code point is not in the table.
int advance_units(const detail::font_table& table, uint32_t cp) {
    const font_advance* end = table.advances + table.advance_count;
    const font_advance* it = std::lower_bound(table.advances, end, cp,
                                              [](const font_advance& a, uint32_t value) { return a.code_point < value; });
    return it != end && it->code_point == cp ? it->units : -1;
}

int kerning_units(const detail::font_table& table, uint32_t a, uint32_t b) {
    if (a > 0xFF || b > 0xFF) return 0;
    const font_kerning* end = table.kerning + table.kerning_count;
    const uint32_t key = a * 256 + b;
    const font_kerning* it = std::lower_bound(table.kerning, end, key, [](const font_kerning& k, uint32_t value) {
        return static_cast<uint32_t>(k.first) * 256 + k.second < value;
    });
    return it != end && static_cast<uint32_t>(it->first) * 256 + it->second == key ? it->units : 0;
}

bool is_combining(uint32_t cp) { return (cp >= 0x0300 && cp <= 0x036F) || cp == 0x200B || cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F); }

}  // namespace

double browser_text_measurer::line_width(std::string_view raw, FontFace face) const {
    // HTML white-space handling: spaces and tabs collapse into one space; the ends of the line lose theirs.
    std::string line;
    for (char c : raw) {
        const bool blank = c == ' ' || c == '\t';
        if (blank && (line.empty() || line.back() == ' ')) continue;
        line += blank ? ' ' : c;
    }
    if (!line.empty() && line.back() == ' ') line.pop_back();
    return run_width(line, face);
}

double browser_text_measurer::run_width(std::string_view line, FontFace face) const {
    const detail::font_table& table = detail::kFontTables[static_cast<int>(face)];
    double units = 0;     // table units
    double fallback = 0;  // em, for characters outside the table
    uint32_t previous = 0;
    for (size_t at = 0; at < line.size();) {
        const uint32_t cp = next_code_point(line, at);
        if (cp < 32 || cp == 0x7F || is_combining(cp)) continue;
        const int advance = advance_units(table, cp);
        if (advance >= 0) {
            units += advance + (previous ? kerning_units(table, previous, cp) : 0);
            previous = cp;
        } else {
            fallback += cp >= 0x2E80 ? kWideFallbackEm : kNarrowFallbackEm;
            previous = 0;
        }
    }
    return (units / kTableUnit) * (style().font_size / kTableFontSize) + fallback * style().font_size;
}

}  // namespace mermaid

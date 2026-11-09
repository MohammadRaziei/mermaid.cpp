#include "svg_text_metrics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "svg_text_metrics_data.h"

namespace mermaid::detail {

namespace {

constexpr double kEpsilon = 1e-9;
constexpr unsigned kWideFrom = 0x2E80;  // CJK and other wide characters: a whole em; any other unknown character: half an em
constexpr double kMinimumHeight = 1;

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

// Math.round
double js_round(double x) { return std::floor(x + 0.5); }

// one face: its design units and what Chromium makes of them at a size
template <size_t glyph_count, size_t pair_count>
class face {
public:
    face(const svg_glyph (&glyphs)[glyph_count], const svg_pair (&pairs)[pair_count], int units_per_em, int ascent, int descent)
        : glyphs_(glyphs), pairs_(pairs), units_per_em_(units_per_em), ascent_(ascent), descent_(descent) {}

    struct run {
        double width = 0;   // getBBox().width, not rounded yet
        double height = 0;  // getBBox().height
        bool empty = true;  // no character at all: calculateTextDimensions then answers with nothing
    };

    // a text as an SVG <text> shows it: runs of blanks collapse and the ones at either end go
    run measure(const std::vector<uint32_t>& raw, double size) const {
        std::vector<uint32_t> cps;
        for (uint32_t cp : raw) {
            if (cp == '\t' || cp == '\n' || cp == '\r') cp = ' ';
            if (cp == ' ' && (cps.empty() || cps.back() == ' ')) continue;
            cps.push_back(cp);
        }
        while (!cps.empty() && cps.back() == ' ') cps.pop_back();
        run r;
        const double k = size / units_per_em_;
        double top = -js_round(ascent_ * k), bottom = js_round(descent_ * k);
        r.height = bottom - top;
        if (cps.empty()) return r;
        r.empty = false;
        double units = 0, right = -std::numeric_limits<double>::infinity(), left = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < cps.size(); ++i) {
            const svg_glyph* g = find_glyph(cps[i]);
            const double origin = units * k;
            if (g && g->ink) {
                right = std::max(right, origin + std::ceil(js_round(g->x_max * k * 64) / 64 - kEpsilon));
                left = std::min(left, origin + std::floor(js_round(g->x_min * k * 64) / 64 + kEpsilon));
                top = std::min(top, -std::ceil(js_round(g->y_max * k * 64) / 64 - kEpsilon));
                bottom = std::max(bottom, -std::floor(js_round(g->y_min * k * 64) / 64 + kEpsilon));
            }
            units += g ? g->advance : (cps[i] >= kWideFrom ? units_per_em_ : units_per_em_ / 2.0);
            if (i + 1 < cps.size()) units += kerning(cps[i], cps[i + 1]);
        }
        const double length = std::ceil(units * k * 64 - kEpsilon) / 64;
        r.width = std::max(length, right) - std::min(0.0, left);
        r.height = std::max(bottom - top, kMinimumHeight);
        return r;
    }

    // the height of the face's box around the characters alone (no collapsing: only the ink matters)
    double height_of(const std::vector<uint32_t>& cps, double size) const {
        const double k = size / units_per_em_;
        double top = -js_round(ascent_ * k), bottom = js_round(descent_ * k);
        for (uint32_t cp : cps) {
            const svg_glyph* g = find_glyph(cp);
            if (!g || !g->ink) continue;
            top = std::min(top, -std::ceil(js_round(g->y_max * k * 64) / 64 - kEpsilon));
            bottom = std::max(bottom, -std::floor(js_round(g->y_min * k * 64) / 64 + kEpsilon));
        }
        return bottom - top;
    }

private:
    const svg_glyph* find_glyph(uint32_t cp) const {
        const svg_glyph* end = glyphs_ + glyph_count;
        const svg_glyph* it = std::lower_bound(glyphs_, end, cp, [](const svg_glyph& g, uint32_t v) { return g.code < v; });
        return it != end && it->code == cp ? it : nullptr;
    }

    int kerning(uint32_t a, uint32_t b) const {
        if (a > 0xFFFF || b > 0xFFFF) return 0;
        const svg_pair* end = pairs_ + pair_count;
        const std::pair<unsigned, unsigned> key(a, b);
        const svg_pair* it = std::lower_bound(pairs_, end, key, [](const svg_pair& p, const std::pair<unsigned, unsigned>& v) {
            return std::pair<unsigned, unsigned>(p.first, p.second) < v;
        });
        return it != end && it->first == a && it->second == b ? it->value : 0;
    }

    const svg_glyph (&glyphs_)[glyph_count];
    const svg_pair (&pairs_)[pair_count];
    int units_per_em_, ascent_, descent_;
};

template <size_t g, size_t p>
face<g, p> make_face(const svg_glyph (&glyphs)[g], const svg_pair (&pairs)[p], int units_per_em, int ascent, int descent) {
    return face<g, p>(glyphs, pairs, units_per_em, ascent, descent);
}

const auto& serif_face() {
    static const auto f = make_face(kSerifGlyphs, kSerifPairs, kSerifUnitsPerEm, kSerifAscent, kSerifDescent);
    return f;
}

const auto& sans_face() {
    static const auto f = make_face(kSansGlyphs, kSansPairs, kSansUnitsPerEm, kSansAscent, kSansDescent);
    return f;
}

template <typename face_type>
svg_text_dimensions single(const face_type& f, std::string_view text, double size) {
    const auto r = f.measure(code_points(text), size);
    if (r.empty) return {};
    return {js_round(r.width), r.height};
}

}  // namespace

svg_text_dimensions svg_text_metrics::serif(std::string_view text, double font_size) { return single(serif_face(), text, font_size); }

svg_text_dimensions svg_text_metrics::sans(std::string_view text, double font_size) { return single(sans_face(), text, font_size); }

double svg_text_metrics::sans_bbox_width(std::string_view text, double font_size) {
    const auto r = sans_face().measure(code_points(text), font_size);
    return r.empty ? 0.0 : r.width;
}

double svg_text_metrics::drawn_height(std::string_view text, double font_size) { return sans_face().height_of(code_points(text), font_size); }

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

svg_text_dimensions svg_text_metrics::calculate(std::string_view text, double font_size) {
    if (text.empty()) return {};  // calculateTextDimensions answers an empty text with nothing at all
    // every row is measured on its own: the width is the widest, the height the sum, the line height the tallest row
    // (an empty row is a zero-width space, as tall as the face's box)
    svg_text_dimensions first, second;
    double first_line = 0, second_line = 0;
    for (const std::string& row : split_breaks(text)) {
        const std::vector<uint32_t> cps = code_points(row);
        const auto s = serif_face().measure(cps, font_size);
        second.width = std::max(second.width, s.empty ? 0.0 : js_round(s.width));
        second.height += s.height;
        second_line = std::max(second_line, s.height);
        const auto a = sans_face().measure(cps, font_size);
        first.width = std::max(first.width, a.empty ? 0.0 : js_round(a.width));
        first.height += a.height;
        first_line = std::max(first_line, a.height);
    }
    // the first family wins only when it is strictly bigger in all three
    const bool first_is_bigger = first.height > second.height && first.width > second.width && first_line > second_line;
    return first_is_bigger ? first : second;
}

}  // namespace mermaid::detail

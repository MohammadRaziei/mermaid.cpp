#include "khroma.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <regex>
#include <stdexcept>

#include <mermaid/css_color.h>

#include "rough_path.h"

namespace mermaid::detail::khroma {
namespace {

enum class channel_type { all, rgb, hsl };

// khroma's `Channels`: the data of one colour with lazy conversion between RGB and HSL.
struct channels {
    std::optional<double> r, g, b, h, s, l;
    double a = 1;
    channel_type type = channel_type::all;
    bool changed = false;
    std::string color;  // the text it was parsed from; returned as is while nothing changed

    // utils/channel.js
    static double hue2rgb(double p, double q, double t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0 / 6) return p + (q - p) * 6 * t;
        if (t < 1.0 / 2) return q;
        if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
        return p;
    }
    double hsl2rgb(char channel) const {
        double hh = *h, ss = *s, ll = *l;
        if (ss == 0) return ll * 2.55;  // `!s`: achromatic
        hh /= 360;
        ss /= 100;
        ll /= 100;
        const double q = ll < 0.5 ? ll * (1 + ss) : (ll + ss) - (ll * ss);
        const double p = 2 * ll - q;
        switch (channel) {
            case 'r': return hue2rgb(p, q, hh + 1.0 / 3) * 255;
            case 'g': return hue2rgb(p, q, hh) * 255;
            default: return hue2rgb(p, q, hh - 1.0 / 3) * 255;
        }
    }
    double rgb2hsl(char channel) const {
        const double rr = *r / 255, gg = *g / 255, bb = *b / 255;
        const double max = std::max({rr, gg, bb}), min = std::min({rr, gg, bb});
        const double ll = (max + min) / 2;
        if (channel == 'l') return ll * 100;
        if (max == min) return 0;
        const double d = max - min;
        const double ss = ll > 0.5 ? d / (2 - max - min) : d / (max + min);
        if (channel == 's') return ss * 100;
        if (max == rr) return ((gg - bb) / d + (gg < bb ? 6 : 0)) * 60;
        if (max == gg) return ((bb - rr) / d + 2) * 60;
        return ((rr - gg) / d + 4) * 60;
    }
    void ensure_hsl() {
        if (!h) h = rgb2hsl('h');
        if (!s) s = rgb2hsl('s');
        if (!l) l = rgb2hsl('l');
    }
    void ensure_rgb() {
        if (!r) r = hsl2rgb('r');
        if (!g) g = hsl2rgb('g');
        if (!b) b = hsl2rgb('b');
    }

    double get(char c) {
        switch (c) {
            case 'r': if (type != channel_type::hsl && r) return *r; ensure_hsl(); return hsl2rgb('r');
            case 'g': if (type != channel_type::hsl && g) return *g; ensure_hsl(); return hsl2rgb('g');
            case 'b': if (type != channel_type::hsl && b) return *b; ensure_hsl(); return hsl2rgb('b');
            case 'h': if (type != channel_type::rgb && h) return *h; ensure_rgb(); return rgb2hsl('h');
            case 's': if (type != channel_type::rgb && s) return *s; ensure_rgb(); return rgb2hsl('s');
            case 'l': if (type != channel_type::rgb && l) return *l; ensure_rgb(); return rgb2hsl('l');
            default: return a;
        }
    }
    void set_type(channel_type t) {
        if (type != channel_type::all && type != t) throw std::runtime_error("Cannot change both RGB and HSL channels at the same time");
        type = t;
    }
    void set(char c, double v) {
        changed = true;
        switch (c) {
            case 'r': set_type(channel_type::rgb); r = v; break;
            case 'g': set_type(channel_type::rgb); g = v; break;
            case 'b': set_type(channel_type::rgb); b = v; break;
            case 'h': set_type(channel_type::hsl); h = v; break;
            case 's': set_type(channel_type::hsl); s = v; break;
            case 'l': set_type(channel_type::hsl); l = v; break;
            default: a = v; break;
        }
    }
};

double clamp(char c, double v) {
    switch (c) {
        case 'r': case 'g': case 'b': return v >= 255 ? 255 : (v < 0 ? 0 : v);
        case 'h': return std::fmod(v, 360);
        case 's': case 'l': return v >= 100 ? 100 : (v < 0 ? 0 : v);
        default: return v >= 1 ? 1 : (v < 0 ? 0 : v);
    }
}

// utils/lang.js `round`: ten decimals, halves up like Math.round.
double round10(double v) { return std::floor(v * 10000000000.0 + 0.5) / 10000000000.0; }

std::string num(double v) { return js_number(round10(v)); }

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

std::optional<channels> parse_hex(const std::string& text) {
    if (text.empty() || text[0] != '#') return std::nullopt;
    const std::string hex = text.substr(1);
    const size_t n = hex.size();
    if (!(n == 3 || n == 4 || n == 6 || n == 8)) return std::nullopt;
    unsigned long dec = 0;
    for (const char c : hex) {
        if (hex_value(c) < 0) return std::nullopt;
        dec = dec * 16 + static_cast<unsigned>(hex_value(c));
    }
    const bool has_alpha = n % 4 == 0, full = n > 4;
    const int multiplier = full ? 1 : 17, bits = full ? 8 : 4, offset = has_alpha ? 0 : -1;
    const unsigned long mask = full ? 255 : 15;
    channels c;
    c.r = static_cast<double>(((dec >> (bits * (offset + 3))) & mask) * static_cast<unsigned long>(multiplier));
    c.g = static_cast<double>(((dec >> (bits * (offset + 2))) & mask) * static_cast<unsigned long>(multiplier));
    c.b = static_cast<double>(((dec >> (bits * (offset + 1))) & mask) * static_cast<unsigned long>(multiplier));
    c.a = has_alpha ? static_cast<double>((dec & mask) * static_cast<unsigned long>(multiplier)) / 255 : 1;
    c.color = text;
    return c;
}

const std::string kNumber = R"((-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e-?\d+)?))";

std::optional<channels> parse_rgb(const std::string& text) {
    static const std::regex re("^rgba?\\(\\s*?" + kNumber + "(%?)\\s*?(?:,|\\s)\\s*?" + kNumber + "(%?)\\s*?(?:,|\\s)\\s*?" + kNumber +
                                   "(%?)(?:\\s*?(?:,|/)\\s*?\\+?" + kNumber + "(%?))?\\s*?\\)$",
                               std::regex::icase);
    std::smatch m;
    if (!std::regex_match(text, m, re)) return std::nullopt;
    channels c;
    const auto chan = [&](int i, char name) {
        const double v = std::strtod(m[static_cast<size_t>(i)].str().c_str(), nullptr);
        return clamp(name, m[static_cast<size_t>(i + 1)].matched && !m[static_cast<size_t>(i + 1)].str().empty() ? v * 2.55 : v);
    };
    c.r = chan(1, 'r');
    c.g = chan(3, 'g');
    c.b = chan(5, 'b');
    if (m[7].matched && !m[7].str().empty()) {
        const double v = std::strtod(m[7].str().c_str(), nullptr);
        c.a = clamp('a', m[8].matched && !m[8].str().empty() ? v / 100 : v);
    }
    c.color = text;
    return c;
}

std::optional<channels> parse_hsl(const std::string& text) {
    static const std::regex re("^hsla?\\(\\s*?" + kNumber + "(deg|grad|rad|turn)?\\s*?(?:,|\\s)\\s*?" + kNumber + "%\\s*?(?:,|\\s)\\s*?" + kNumber +
                                   "%(?:\\s*?(?:,|/)\\s*?\\+?" + kNumber + "(%)?)?\\s*?\\)$",
                               std::regex::icase);
    std::smatch m;
    if (!std::regex_match(text, m, re)) return std::nullopt;
    channels c;
    double hue = std::strtod(m[1].str().c_str(), nullptr);
    std::string unit = m[2].str();
    for (char& ch : unit) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (unit == "grad") hue *= 0.9;
    else if (unit == "rad") hue = hue * 180 / 3.14159265358979323846;
    else if (unit == "turn") hue *= 360;
    c.h = clamp('h', hue);
    c.s = clamp('s', std::strtod(m[3].str().c_str(), nullptr));
    c.l = clamp('l', std::strtod(m[4].str().c_str(), nullptr));
    if (m[5].matched && !m[5].str().empty()) {
        const double v = std::strtod(m[5].str().c_str(), nullptr);
        c.a = clamp('a', m[6].matched && !m[6].str().empty() ? v / 100 : v);
    }
    c.color = text;
    return c;
}

std::string hex2(double v) {
    static const char* digits = "0123456789abcdef";
    const int i = static_cast<int>(std::floor(v + 0.5));
    return std::string{digits[i / 16], digits[i % 16]};
}

channels parse(const std::string& text) {
    if (auto c = parse_hex(text)) return *c;
    if (auto c = parse_rgb(text)) return *c;
    if (auto c = parse_hsl(text)) return *c;
    if (const auto keyword = parse_css_color(text); keyword && !text.empty() && std::isalpha(static_cast<unsigned char>(text[0]))) {
        // Keyword: khroma looks the name up as a hex string and parses that, which becomes the text of the colour.
        if (auto c = parse_hex("#" + hex2(keyword->r) + hex2(keyword->g) + hex2(keyword->b))) return *c;
    }
    throw std::invalid_argument("Unsupported color format: \"" + text + "\"");
}

// color/index.js `stringify`
std::string stringify(channels& c) {
    if (!c.changed && !c.color.empty()) return c.color;
    if (c.type == channel_type::hsl || !c.r) {
        const double h = c.get('h'), s = c.get('s'), l = c.get('l');
        if (c.a < 1) return "hsla(" + num(h) + ", " + num(s) + "%, " + num(l) + "%, " + js_number(c.a) + ")";
        return "hsl(" + num(h) + ", " + num(s) + "%, " + num(l) + "%)";
    }
    const double r = c.get('r'), g = c.get('g'), b = c.get('b');
    const auto integer = [](double v) { return v == std::floor(v); };
    if (c.a < 1 || !integer(r) || !integer(g) || !integer(b)) {
        if (c.a < 1) return "rgba(" + num(r) + ", " + num(g) + ", " + num(b) + ", " + num(c.a) + ")";
        return "rgb(" + num(r) + ", " + num(g) + ", " + num(b) + ")";
    }
    if (c.a < 1) return "#" + hex2(r) + hex2(g) + hex2(b) + hex2(c.a * 255);
    return "#" + hex2(r) + hex2(g) + hex2(b);
}

double to_linear(double c) {
    const double n = c / 255;
    return c > .03928 ? std::pow((n + .055) / 1.055, 2.4) : n / 12.92;  // khroma compares c, not n; kept
}

double luminance(const std::string& color) {
    channels c = parse(color);
    return round10(.2126 * to_linear(c.get('r')) + .7152 * to_linear(c.get('g')) + .0722 * to_linear(c.get('b')));
}

// methods/mix.js
std::string mix(channels& first, const std::string& second, double weight) {
    const double r1 = first.get('r'), g1 = first.get('g'), b1 = first.get('b'), a1 = first.a;
    channels other = parse(second);
    const double r2 = other.get('r'), g2 = other.get('g'), b2 = other.get('b'), a2 = other.a;
    const double weight_scale = weight / 100, weight_normalized = weight_scale * 2 - 1, alpha_delta = a1 - a2;
    const double combined = weight_normalized * alpha_delta == -1 ? weight_normalized : (weight_normalized + alpha_delta) / (1 + weight_normalized * alpha_delta);
    const double weight1 = (combined + 1) / 2, weight2 = 1 - weight1;
    return rgba(r1 * weight1 + r2 * weight2, g1 * weight1 + g2 * weight2, b1 * weight1 + b2 * weight2, a1 * weight_scale + a2 * (1 - weight_scale));
}

std::string adjust_channel(const std::string& color, char channel, double amount) {
    channels c = parse(color);
    const double current = c.get(channel), next = clamp(channel, current + amount);
    if (current != next) c.set(channel, next);
    return stringify(c);
}

}  // namespace

std::string adjust(const std::string& color, std::initializer_list<channel_change> changes) {
    channels first = parse(color);
    std::vector<channel_change> next;
    for (const auto& [name, delta] : changes)
        if (delta != 0) next.emplace_back(name, first.get(name) + delta);
    channels c = parse(color);  // `change` parses again
    for (const auto& [name, value] : next) c.set(name, clamp(name, value));
    return stringify(c);
}

std::string lighten(const std::string& color, double amount) { return adjust_channel(color, 'l', amount); }
std::string darken(const std::string& color, double amount) { return adjust_channel(color, 'l', -amount); }

std::string invert(const std::string& color) {
    channels inverse = parse(color);
    inverse.set('r', 255 - inverse.get('r'));
    inverse.set('g', 255 - inverse.get('g'));
    inverse.set('b', 255 - inverse.get('b'));
    return mix(inverse, color, 100);
}

std::string rgba(double r, double g, double b, double a) {
    channels c;
    c.r = clamp('r', r);
    c.g = clamp('g', g);
    c.b = clamp('b', b);
    c.a = clamp('a', a);
    return stringify(c);
}

bool is_dark(const std::string& color) { return !(luminance(color) >= .5); }

std::string mk_border(const std::string& color, bool dark_mode) {
    return dark_mode ? adjust(color, {{'s', -40}, {'l', 10}}) : adjust(color, {{'s', -40}, {'l', -10}});
}

}  // namespace mermaid::detail::khroma

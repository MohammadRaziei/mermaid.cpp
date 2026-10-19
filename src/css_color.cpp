#include <mermaid/css_color.h>

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace mermaid {

namespace {

int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(c | 32);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

std::string number(double v) {
    std::ostringstream out;
    out << v;
    return out.str();
}

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

// Splits "a, b, c" in the parentheses of `text` (after `prefix(`).
bool arguments(const std::string& text, const std::string& prefix, double out[3]) {
    if (text.compare(0, prefix.size() + 1, prefix + "(") != 0 || text.back() != ')') return false;
    const std::string inner = text.substr(prefix.size() + 1, text.size() - prefix.size() - 2);
    size_t start = 0;
    for (int i = 0; i < 3; ++i) {
        const size_t comma = inner.find(',', start);
        if ((comma == std::string::npos) != (i == 2)) return false;
        const std::string part = trim(inner.substr(start, comma == std::string::npos ? comma : comma - start));
        if (part.empty()) return false;
        char* end = nullptr;
        out[i] = std::strtod(part.c_str(), &end);
        if (end == part.c_str()) return false;
        start = comma + 1;
    }
    return true;
}

double hue_channel(double p, double q, double t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1.0 / 6) return p + (q - p) * 6 * t;
    if (t < 0.5) return q;
    if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
    return p;
}

}  // namespace

std::optional<rgb_color> parse_css_color(std::string_view input) {
    const std::string text = trim(input);
    if (text.size() >= 2 && text[0] == '#') {
        std::string digits = text.substr(1);
        if (digits.size() == 3) digits = std::string{digits[0], digits[0], digits[1], digits[1], digits[2], digits[2]};
        if (digits.size() != 6) return std::nullopt;
        int v[6];
        for (int i = 0; i < 6; ++i) {
            v[i] = hex_digit(digits[static_cast<size_t>(i)]);
            if (v[i] < 0) return std::nullopt;
        }
        return rgb_color{v[0] * 16 + v[1], v[2] * 16 + v[3], v[4] * 16 + v[5]};
    }
    double a[3];
    if (arguments(text, "rgb", a)) return rgb_color{static_cast<int>(std::lround(a[0])), static_cast<int>(std::lround(a[1])), static_cast<int>(std::lround(a[2]))};
    if (arguments(text, "hsl", a)) {
        double h = std::fmod(a[0], 360);
        if (h < 0) h += 360;
        h /= 360;
        const double s = a[1] / 100, l = a[2] / 100;
        double r = l, g = l, b = l;
        if (s != 0) {
            const double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
            const double p = 2 * l - q;
            r = hue_channel(p, q, h + 1.0 / 3);
            g = hue_channel(p, q, h);
            b = hue_channel(p, q, h - 1.0 / 3);
        }
        return rgb_color{static_cast<int>(std::lround(r * 255)), static_cast<int>(std::lround(g * 255)), static_cast<int>(std::lround(b * 255))};
    }
    return std::nullopt;
}

std::string css_rgb(const rgb_color& c) {
    return "rgb(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ")";
}

std::string css_rgba(const rgb_color& c, double alpha) {
    return "rgba(" + std::to_string(c.r) + ", " + std::to_string(c.g) + ", " + std::to_string(c.b) + ", " + number(alpha) + ")";
}

std::string fade_color(std::string_view color, double alpha) {
    const std::optional<rgb_color> parsed = parse_css_color(color);
    return parsed ? css_rgba(*parsed, alpha) : std::string(color);
}

}  // namespace mermaid

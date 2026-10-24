#include <mermaid/css_color.h>

#include <cctype>
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

struct named_color {
    const char* name;
    unsigned rgb;
};

// CSS colour keywords as khroma knows them (khroma/dist/color/keyword.js).
const named_color kNamedColors[] = {
    {"aliceblue", 0xf0f8ff},
    {"antiquewhite", 0xfaebd7},
    {"aqua", 0x00ffff},
    {"aquamarine", 0x7fffd4},
    {"azure", 0xf0ffff},
    {"beige", 0xf5f5dc},
    {"bisque", 0xffe4c4},
    {"black", 0x000000},
    {"blanchedalmond", 0xffebcd},
    {"blue", 0x0000ff},
    {"blueviolet", 0x8a2be2},
    {"brown", 0xa52a2a},
    {"burlywood", 0xdeb887},
    {"cadetblue", 0x5f9ea0},
    {"chartreuse", 0x7fff00},
    {"chocolate", 0xd2691e},
    {"coral", 0xff7f50},
    {"cornflowerblue", 0x6495ed},
    {"cornsilk", 0xfff8dc},
    {"crimson", 0xdc143c},
    {"cyanaqua", 0x00ffff},
    {"darkblue", 0x00008b},
    {"darkcyan", 0x008b8b},
    {"darkgoldenrod", 0xb8860b},
    {"darkgray", 0xa9a9a9},
    {"darkgreen", 0x006400},
    {"darkgrey", 0xa9a9a9},
    {"darkkhaki", 0xbdb76b},
    {"darkmagenta", 0x8b008b},
    {"darkolivegreen", 0x556b2f},
    {"darkorange", 0xff8c00},
    {"darkorchid", 0x9932cc},
    {"darkred", 0x8b0000},
    {"darksalmon", 0xe9967a},
    {"darkseagreen", 0x8fbc8f},
    {"darkslateblue", 0x483d8b},
    {"darkslategray", 0x2f4f4f},
    {"darkslategrey", 0x2f4f4f},
    {"darkturquoise", 0x00ced1},
    {"darkviolet", 0x9400d3},
    {"deeppink", 0xff1493},
    {"deepskyblue", 0x00bfff},
    {"dimgray", 0x696969},
    {"dimgrey", 0x696969},
    {"dodgerblue", 0x1e90ff},
    {"firebrick", 0xb22222},
    {"floralwhite", 0xfffaf0},
    {"forestgreen", 0x228b22},
    {"fuchsia", 0xff00ff},
    {"gainsboro", 0xdcdcdc},
    {"ghostwhite", 0xf8f8ff},
    {"gold", 0xffd700},
    {"goldenrod", 0xdaa520},
    {"gray", 0x808080},
    {"green", 0x008000},
    {"greenyellow", 0xadff2f},
    {"grey", 0x808080},
    {"honeydew", 0xf0fff0},
    {"hotpink", 0xff69b4},
    {"indianred", 0xcd5c5c},
    {"indigo", 0x4b0082},
    {"ivory", 0xfffff0},
    {"khaki", 0xf0e68c},
    {"lavender", 0xe6e6fa},
    {"lavenderblush", 0xfff0f5},
    {"lawngreen", 0x7cfc00},
    {"lemonchiffon", 0xfffacd},
    {"lightblue", 0xadd8e6},
    {"lightcoral", 0xf08080},
    {"lightcyan", 0xe0ffff},
    {"lightgoldenrodyellow", 0xfafad2},
    {"lightgray", 0xd3d3d3},
    {"lightgreen", 0x90ee90},
    {"lightgrey", 0xd3d3d3},
    {"lightpink", 0xffb6c1},
    {"lightsalmon", 0xffa07a},
    {"lightseagreen", 0x20b2aa},
    {"lightskyblue", 0x87cefa},
    {"lightslategray", 0x778899},
    {"lightslategrey", 0x778899},
    {"lightsteelblue", 0xb0c4de},
    {"lightyellow", 0xffffe0},
    {"lime", 0x00ff00},
    {"limegreen", 0x32cd32},
    {"linen", 0xfaf0e6},
    {"magenta", 0xff00ff},
    {"maroon", 0x800000},
    {"mediumaquamarine", 0x66cdaa},
    {"mediumblue", 0x0000cd},
    {"mediumorchid", 0xba55d3},
    {"mediumpurple", 0x9370db},
    {"mediumseagreen", 0x3cb371},
    {"mediumslateblue", 0x7b68ee},
    {"mediumspringgreen", 0x00fa9a},
    {"mediumturquoise", 0x48d1cc},
    {"mediumvioletred", 0xc71585},
    {"midnightblue", 0x191970},
    {"mintcream", 0xf5fffa},
    {"mistyrose", 0xffe4e1},
    {"moccasin", 0xffe4b5},
    {"navajowhite", 0xffdead},
    {"navy", 0x000080},
    {"oldlace", 0xfdf5e6},
    {"olive", 0x808000},
    {"olivedrab", 0x6b8e23},
    {"orange", 0xffa500},
    {"orangered", 0xff4500},
    {"orchid", 0xda70d6},
    {"palegoldenrod", 0xeee8aa},
    {"palegreen", 0x98fb98},
    {"paleturquoise", 0xafeeee},
    {"palevioletred", 0xdb7093},
    {"papayawhip", 0xffefd5},
    {"peachpuff", 0xffdab9},
    {"peru", 0xcd853f},
    {"pink", 0xffc0cb},
    {"plum", 0xdda0dd},
    {"powderblue", 0xb0e0e6},
    {"purple", 0x800080},
    {"rebeccapurple", 0x663399},
    {"red", 0xff0000},
    {"rosybrown", 0xbc8f8f},
    {"royalblue", 0x4169e1},
    {"saddlebrown", 0x8b4513},
    {"salmon", 0xfa8072},
    {"sandybrown", 0xf4a460},
    {"seagreen", 0x2e8b57},
    {"seashell", 0xfff5ee},
    {"sienna", 0xa0522d},
    {"silver", 0xc0c0c0},
    {"skyblue", 0x87ceeb},
    {"slateblue", 0x6a5acd},
    {"slategray", 0x708090},
    {"slategrey", 0x708090},
    {"snow", 0xfffafa},
    {"springgreen", 0x00ff7f},
    {"tan", 0xd2b48c},
    {"teal", 0x008080},
    {"thistle", 0xd8bfd8},
    {"turquoise", 0x40e0d0},
    {"violet", 0xee82ee},
    {"wheat", 0xf5deb3},
    {"white", 0xffffff},
    {"whitesmoke", 0xf5f5f5},
    {"yellow", 0xffff00},
    {"yellowgreen", 0x9acd32}
};

std::optional<rgb_color> keyword_color(const std::string& text) {
    std::string lower;
    for (const char ch : text) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (const named_color& n : kNamedColors)
        if (lower == n.name) return rgb_color{static_cast<int>(n.rgb >> 16 & 0xFF), static_cast<int>(n.rgb >> 8 & 0xFF), static_cast<int>(n.rgb & 0xFF)};
    return std::nullopt;
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
    return keyword_color(text);
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

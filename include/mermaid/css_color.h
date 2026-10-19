// css_color.h - the few CSS colour conversions Mermaid's stylesheet needs.
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mermaid {

struct rgb_color {
    int r = 0;
    int g = 0;
    int b = 0;
};

// Parses `#rgb`, `#rrggbb`, `rgb(r, g, b)` and `hsl(h, s%, l%)` (hue may be negative).
std::optional<rgb_color> parse_css_color(std::string_view text);

// "rgb(221, 238, 255)" - the form a browser reports for a colour set on an element.
std::string css_rgb(const rgb_color& color);

// "rgba(204, 204, 204, 0.5)" - khroma's `rgba(r, g, b, a)`.
std::string css_rgba(const rgb_color& color, double alpha);

// khroma-style fade: the colour with the given opacity. Returns the input when it cannot be parsed.
std::string fade_color(std::string_view color, double alpha);

}  // namespace mermaid

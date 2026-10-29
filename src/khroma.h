// khroma.h (private) - the colour arithmetic Mermaid's themes use, ported from khroma 2.1 (the library Mermaid bundles).
//
// Every function takes and returns CSS colour strings exactly like khroma does, so a derived colour is the same text
// (`hsl(40.5882352941, 60%, 83.3333333333%)`, `rgb(8.5000000002, 5.7500000001, 0)`, `#cccccc`) that Mermaid writes.
#pragma once

#include <initializer_list>
#include <string>
#include <utility>

namespace mermaid::detail::khroma {

// A change of one channel: 'r' 'g' 'b' (0..255), 'h' (degrees), 's' 'l' (percent) or 'a' (0..1).
using channel_change = std::pair<char, double>;

// khroma `adjust`: add the amounts to the channels (all RGB or all HSL, as khroma insists), clamp, write the colour.
std::string adjust(const std::string& color, std::initializer_list<channel_change> changes);
std::string lighten(const std::string& color, double amount);
std::string darken(const std::string& color, double amount);
// khroma `invert` with the default weight of 100.
std::string invert(const std::string& color);
// khroma `rgba(r, g, b, a)`.
std::string rgba(double r, double g, double b, double a);
// khroma `isDark`: the relative luminance is below one half.
bool is_dark(const std::string& color);
// theme-helpers.js `mkBorder`.
std::string mk_border(const std::string& color, bool dark_mode);

}  // namespace mermaid::detail::khroma

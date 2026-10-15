#include "text_measurer.h"

#include <algorithm>

namespace mermaidcpp {

Size TextMeasurer::measure(std::string_view text) const {
    double width = 0;
    int lines = 0;
    size_t start = 0;
    for (;;) {
        const size_t nl = text.find('\n', start);
        const std::string_view line =
            text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        width = std::max(width, line_width(line));
        ++lines;
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return {width, lines * style_.line_height()};
}

}  // namespace mermaidcpp

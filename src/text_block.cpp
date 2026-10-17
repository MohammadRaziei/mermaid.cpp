#include "text_block.h"

#include <vector>

namespace mermaid::detail {

void write_text_block(SvgWriter& out, const Point& center, std::string_view text, double line_height,
                      const std::string& style) {
    std::vector<std::string_view> lines;
    size_t start = 0;
    while (true) {
        const size_t end = text.find('\n', start);
        lines.push_back(text.substr(start, end == std::string_view::npos ? end : end - start));
        if (end == std::string_view::npos) break;
        start = end + 1;
    }

    SvgWriter::Attrs attrs{{"text-anchor", "middle"}};
    if (!style.empty()) attrs.emplace_back("style", style);
    out.open("text", attrs);
    // y of a tspan is its baseline: sit it a little below the middle of its line.
    const double baseline = line_height * 0.23;
    const double first = center.y - line_height * static_cast<double>(lines.size() - 1) / 2 + baseline;
    for (size_t i = 0; i < lines.size(); ++i)
        out.element("tspan", {{"x", SvgWriter::num(center.x)}, {"y", SvgWriter::num(first + line_height * static_cast<double>(i))}},
                    lines[i]);
    out.close();
}

}  // namespace mermaid::detail

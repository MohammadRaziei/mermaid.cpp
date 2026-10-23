#include <mermaid/text_measurer.h>

#include <string>

#include <algorithm>
#include <mermaid/macros.h>

namespace mermaid {

Size TextMeasurer::measure(std::string_view text) const {
    double width = 0;
    int lines = 0;
    size_t start = 0;
    for (;;) {
        const size_t nl = text.find('\n', start);
        const std::string_view line =
            text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        width = MERMAID_MAX(width, line_width(line));
        ++lines;
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    return {width, lines * style_.line_height()};
}

TextMeasurer::Wrapped TextMeasurer::measure_wrapped(std::string_view text, double max_width) const {
    Wrapped out;
    double widest = 0;
    int lines = 0;
    size_t start = 0;
    for (;;) {
        const size_t nl = text.find('\n', start);
        const std::string_view line =
            text.substr(start, nl == std::string_view::npos ? std::string_view::npos : nl - start);
        const double natural = line_width(line);
        if (natural <= max_width) {
            widest = MERMAID_MAX(widest, natural);
            ++lines;
        } else {
            out.wrapped = true;
            std::string current;
            size_t pos = 0;
            while (pos <= line.size()) {
                const size_t space = line.find(' ', pos);
                const std::string word(line.substr(pos, space == std::string_view::npos ? std::string_view::npos : space - pos));
                pos = space == std::string_view::npos ? line.size() + 1 : space + 1;
                if (word.empty()) continue;
                const std::string candidate = current.empty() ? word : current + " " + word;
                if (current.empty() || line_width(candidate) <= max_width) {
                    current = candidate;
                } else {
                    ++lines;
                    current = word;
                }
            }
            ++lines;  // the last line of this paragraph
        }
        if (nl == std::string_view::npos) break;
        start = nl + 1;
    }
    out.size = {out.wrapped ? max_width : widest, lines * style_.line_height()};
    return out;
}

}  // namespace mermaid

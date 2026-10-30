// markdown_label.h (private) - the Markdown of a `"`...`"` label: bold and italic runs, line breaks.
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <mermaid/text_measurer.h>

namespace mermaid::detail {

struct markdown_label_run {
    std::string text;
    FontFace face = FontFace::Regular;
};

// What Mermaid's markdownToHTML makes of a label, for the part of Markdown the flowchart labels use: `**strong**`,
// `__strong__`, `*em*`, `_em_`, `***both***` and line breaks. Other Markdown (lists, links, code) stays text.
class markdown_label {
public:
    explicit markdown_label(std::string_view text);

    // `<p>...</p>` with `<strong>` / `<em>`, lines joined by `<br />`, text escaped.
    const std::string& html() const { return html_; }

    // createText's measuring: wraps at `max_width` like TextMeasurer::measure_wrapped, with the width of every run in
    // its own face.
    TextMeasurer::Wrapped measure(const TextMeasurer& measurer, double max_width) const;

    // The words of every line, each in its face (markdownToLines): what an SVG label turns into <tspan>s.
    std::vector<std::vector<TextWord>> words() const;

private:
    using line = std::vector<markdown_label_run>;

    std::vector<line> lines_;
    std::string html_;
};

}  // namespace mermaid::detail

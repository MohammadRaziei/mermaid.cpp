// approx_text_measurer.h - zero-dependency measurer using Arial/Helvetica metrics.
#pragma once

#include <mermaid/text_measurer.h>

namespace mermaid {

// Per-character advance widths of Arial (== Helvetica AFM, 1000 units/em) for
// printable ASCII. Real Mermaid in a browser fell back to Arial in the
// upstream fixtures, so this is within ~0.1 px of it for kern-free text.
// No kerning; non-ASCII uses coarse fallbacks.
class ApproxTextMeasurer final : public TextMeasurer {
public:
    explicit ApproxTextMeasurer(TextStyle style = {}) : TextMeasurer(style) {}
    double line_width(std::string_view line) const override;
};

}  // namespace mermaid

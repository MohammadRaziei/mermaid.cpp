// browser_font_metrics.h (private) - generated tables, see tools/dump_font_metrics.mjs.
#pragma once

#include <cstddef>
#include <cstdint>

namespace mermaid::detail {

struct font_advance {
    uint32_t code_point;
    int16_t units;  // 1/64 px at 14px
};

struct font_kerning {
    uint8_t first;
    uint8_t second;
    int8_t units;  // 1/64 px at 14px
};

extern const font_advance kFontAdvances[];  // sorted by code point
extern const size_t kFontAdvanceCount;
extern const font_kerning kFontKerning[];  // sorted by (first, second)
extern const size_t kFontKerningCount;

}  // namespace mermaid::detail

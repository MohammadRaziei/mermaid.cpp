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

// One face: advances sorted by code point, kerning sorted by (first, second).
struct font_table {
    const font_advance* advances;
    size_t advance_count;
    const font_kerning* kerning;
    size_t kerning_count;
};

// Regular, Bold, Italic, BoldItalic (the order of `FontFace`).
extern const font_table kFontTables[4];

}  // namespace mermaid::detail

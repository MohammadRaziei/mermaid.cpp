#include <mermaid/approx_text_measurer.h>

#include <array>

namespace mermaid {
namespace {

// Advance widths for code points 32..126, in 1/1000 em.
constexpr std::array<int, 95> kAscii = {
    278, 278, 355, 556, 556, 889, 667, 191, 333, 333, 389, 584, 278, 333, 278, 278,   // sp ! " # $ % & ' ( ) * + , - . /
    556, 556, 556, 556, 556, 556, 556, 556, 556, 556, 278, 278, 584, 584, 584, 556,   // 0-9 : ; < = > ?
    1015, 667, 667, 722, 722, 667, 611, 778, 722, 278, 500, 667, 556, 833, 722, 778,  // @ A-O
    667, 778, 722, 667, 611, 722, 667, 944, 667, 667, 611, 278, 278, 278, 469, 556,   // P-Z [ \ ] ^ _
    333, 556, 556, 500, 556, 556, 278, 556, 556, 222, 222, 500, 222, 833, 556, 556,   // ` a-o
    556, 556, 333, 500, 278, 556, 500, 722, 500, 500, 500, 334, 260, 334, 584,        // p-z { | } ~
};

constexpr int kLatin2ByteFallback = 556;  // average Latin letter
constexpr int kWideFallback = 1000;       // CJK and other 3/4-byte sequences: 1 em

}  // namespace

double ApproxTextMeasurer::line_width(std::string_view line) const {
    long units = 0;
    for (unsigned char c : line) {
        if (c >= 32 && c <= 126) units += kAscii[c - 32];
        else if (c >= 0xE0) units += kWideFallback;           // lead byte of a 3/4-byte sequence
        else if (c >= 0xC0) units += kLatin2ByteFallback;     // lead byte of a 2-byte sequence
        // control characters and UTF-8 continuation bytes add nothing
    }
    return static_cast<double>(units) * style().font_size / 1000.0;
}

}  // namespace mermaid

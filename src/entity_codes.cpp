#include "entity_codes.h"

#include <cstdint>
#include <map>

namespace mermaid::detail {
namespace {

const char* const kOpen = "\xEF\xAC\x82\xC2\xB0";   // ﬂ°
const char* const kClose = "\xC2\xB6\xC3\x9F";       // ¶ß

std::string utf8(uint32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

const std::map<std::string, uint32_t>& table() {
    static const std::map<std::string, uint32_t> t = {
        {"amp", 38}, {"lt", 60}, {"gt", 62}, {"quot", 34}, {"apos", 39}, {"nbsp", 160}, {"copy", 169}, {"reg", 174}, {"trade", 8482},
        {"hellip", 8230}, {"mdash", 8212}, {"ndash", 8211}, {"lsquo", 8216}, {"rsquo", 8217}, {"ldquo", 8220}, {"rdquo", 8221},
        {"laquo", 171}, {"raquo", 187}, {"bull", 8226}, {"middot", 183}, {"deg", 176}, {"plusmn", 177}, {"times", 215},
        {"divide", 247}, {"frac12", 189}, {"frac14", 188}, {"frac34", 190}, {"larr", 8592}, {"uarr", 8593}, {"rarr", 8594},
        {"darr", 8595}, {"harr", 8596}, {"lArr", 8656}, {"rArr", 8658}, {"hearts", 9829}, {"spades", 9824}, {"clubs", 9827},
        {"diams", 9830}, {"infin", 8734}, {"ne", 8800}, {"le", 8804}, {"ge", 8805}, {"asymp", 8776}, {"equiv", 8801}, {"sum", 8721},
        {"prod", 8719}, {"radic", 8730}, {"part", 8706}, {"nabla", 8711}, {"isin", 8712}, {"cap", 8745}, {"cup", 8746},
        {"int", 8747}, {"forall", 8704}, {"exist", 8707}, {"empty", 8709}, {"and", 8743}, {"or", 8744}, {"not", 172},
        {"alpha", 945}, {"beta", 946}, {"gamma", 947}, {"delta", 948}, {"epsilon", 949}, {"zeta", 950}, {"eta", 951}, {"theta", 952},
        {"iota", 953}, {"kappa", 954}, {"lambda", 955}, {"mu", 956}, {"nu", 957}, {"xi", 958}, {"omicron", 959}, {"pi", 960},
        {"rho", 961}, {"sigma", 963}, {"tau", 964}, {"upsilon", 965}, {"phi", 966}, {"chi", 967}, {"psi", 968}, {"omega", 969},
        {"Gamma", 915}, {"Delta", 916}, {"Theta", 920}, {"Lambda", 923}, {"Pi", 928}, {"Sigma", 931}, {"Phi", 934}, {"Psi", 936},
        {"Omega", 937}, {"euro", 8364}, {"pound", 163}, {"yen", 165}, {"cent", 162}, {"sect", 167}, {"para", 182}, {"dagger", 8224},
        {"Dagger", 8225}, {"permil", 8240}, {"prime", 8242}, {"Prime", 8243}, {"check", 10003}, {"star", 9734}, {"starf", 9733},
        {"semi", 59}, {"num", 35}, {"colon", 58}, {"comma", 44}, {"period", 46}, {"excl", 33}, {"quest", 63}, {"lpar", 40},
        {"rpar", 41}, {"lbrack", 91}, {"rbrack", 93}, {"lbrace", 123}, {"rbrace", 125}, {"sol", 47}, {"bsol", 92}, {"verbar", 124},
        {"ast", 42}, {"plus", 43}, {"equals", 61}, {"percnt", 37}, {"dollar", 36}, {"commat", 64}, {"hat", 94}, {"lowbar", 95},
        {"grave", 96}, {"tilde", 126}, {"iexcl", 161}, {"iquest", 191}, {"szlig", 223}, {"eacute", 233}, {"egrave", 232},
        {"agrave", 224}, {"aacute", 225}, {"auml", 228}, {"ouml", 246}, {"uuml", 252}, {"ntilde", 241}, {"ccedil", 231},
    };
    return t;
}

}  // namespace

std::string decode_entity_codes(const std::string& text) {
    std::string out;
    const std::string open = kOpen, close = kClose;
    for (size_t i = 0; i < text.size();) {
        if (text.compare(i, open.size(), open) != 0) {
            out += text[i++];
            continue;
        }
        const size_t end = text.find(close, i + open.size());
        if (end == std::string::npos) {
            out += text[i++];
            continue;
        }
        std::string inner = text.substr(i + open.size(), end - i - open.size());
        const bool numeric = inner.size() > 2 && inner.compare(0, 2, "\xC2\xB0") == 0;  // the second °
        if (numeric) {
            const std::string digits = inner.substr(2);
            const unsigned long cp = digits.empty() ? 0 : std::stoul(digits);
            out += cp == 0 || cp > 0x10FFFF ? utf8(0xFFFD) : utf8(static_cast<uint32_t>(cp));
        } else {
            const auto it = table().find(inner);
            out += it != table().end() ? utf8(it->second) : "&" + inner + ";";
        }
        i = end + close.size();
    }
    return out;
}

}  // namespace mermaid::detail

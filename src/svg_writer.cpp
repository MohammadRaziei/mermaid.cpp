#include "svg_writer.h"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace mermaid::detail {

std::string SvgWriter::num(double value) {
    if (!std::isfinite(value)) return "0";
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.3f", value);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0" || s.empty()) return "0";
    return s;
}

std::string SvgWriter::escape(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

void SvgWriter::write_attrs(const Attrs& attrs) {
    for (const auto& [name, value] : attrs) {
        out_ += ' ';
        out_ += name;
        out_ += "=\"";
        out_ += escape(value);
        out_ += '"';
    }
}

SvgWriter& SvgWriter::open(std::string_view tag, const Attrs& attrs) {
    out_ += '<';
    out_ += tag;
    write_attrs(attrs);
    out_ += '>';
    open_.emplace_back(tag);
    return *this;
}

SvgWriter& SvgWriter::close() {
    if (open_.empty()) throw std::logic_error("SvgWriter: no open tag to close");
    out_ += "</" + open_.back() + '>';
    open_.pop_back();
    return *this;
}

SvgWriter& SvgWriter::leaf(std::string_view tag, const Attrs& attrs) {
    out_ += '<';
    out_ += tag;
    write_attrs(attrs);
    out_ += "/>";
    return *this;
}

SvgWriter& SvgWriter::text(std::string_view characters) {
    out_ += escape(characters);
    return *this;
}

SvgWriter& SvgWriter::raw(std::string_view markup) {
    out_ += markup;
    return *this;
}

SvgWriter& SvgWriter::element(std::string_view tag, const Attrs& attrs, std::string_view characters) {
    open(tag, attrs);
    text(characters);
    return close();
}

std::string SvgWriter::str() const {
    if (!open_.empty()) throw std::logic_error("SvgWriter: <" + open_.back() + "> is still open");
    return out_;
}

}  // namespace mermaid::detail

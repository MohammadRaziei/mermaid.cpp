// svg_writer.h (private) - tiny XML builder: escaping, numbers, balanced tags.
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mermaid::detail {

class SvgWriter {
public:
    using Attrs = std::vector<std::pair<std::string, std::string>>;

    SvgWriter& open(std::string_view tag, const Attrs& attrs = {});
    SvgWriter& close();                                    // closes the innermost open tag
    SvgWriter& leaf(std::string_view tag, const Attrs& attrs = {});  // <tag .../>
    SvgWriter& text(std::string_view characters);          // escaped
    SvgWriter& raw(std::string_view markup);               // written as is (CSS)
    SvgWriter& element(std::string_view tag, const Attrs& attrs, std::string_view characters);
    SvgWriter& paragraph(std::string_view lines);          // <p> with <br /> between the lines of the text (createText)

    // Throws std::logic_error when a tag is still open.
    std::string str() const;

    static std::string num(double value);                  // shortest form, never nan/inf
    static std::string escape(std::string_view s);

private:
    void write_attrs(const Attrs& attrs);

    std::string out_;
    std::vector<std::string> open_;
};

}  // namespace mermaid::detail

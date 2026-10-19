// css_compiler.h - flattens and minifies Mermaid's nested style templates.
//
// Mermaid builds each diagram's stylesheet from nested templates (`.a { p { ... } }`, `&`) and
// compiles it with stylis under the namespace `#<svg id>`. css_compiler reproduces that output:
// every rule is prefixed with the namespace, nested rules follow their parent's declarations,
// comments and insignificant whitespace are removed and each declaration ends with `;`.
#pragma once

#include <string>

namespace mermaid {

class css_compiler {
public:
    // `ns` is the namespace selector (e.g. "#my-svg"); `css` the template body.
    static std::string compile(const std::string& ns, const std::string& css);
};

}  // namespace mermaid

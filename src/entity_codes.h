// entity_codes.h (private) - the `#35;` / `#infin;` entity codes of Mermaid text, as the renderer turns them back into characters.
#pragma once

#include <string>

namespace mermaid::detail {

// encodeEntities turns `#35;` into `ﬂ°°35¶ß` and `#infin;` into `ﬂ°infin¶ß` before parsing; the browser then reads the decoded
// `&#35;` / `&infin;` as text. Returns `text` with those placeholders replaced by the UTF-8 characters; a name that is not in the
// (common) table stays as the literal `&name;`, which is what an HTML parser makes of an unknown name.
std::string decode_entity_codes(const std::string& text);

}  // namespace mermaid::detail

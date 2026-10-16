#include <mermaid/mermaid.h>

namespace mermaid {

const char* version() noexcept { return MERMAID_VERSION_STRING; }

const char* mermaid_reference_version() noexcept {
    return MERMAID_REFERENCE_VERSION;
}

}  // namespace mermaid

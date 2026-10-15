#include "mermaidcpp.h"

namespace mermaidcpp {

const char* version() noexcept { return MERMAIDCPP_VERSION_STRING; }

const char* mermaid_reference_version() noexcept {
    return MERMAIDCPP_MERMAID_REFERENCE_VERSION;
}

}  // namespace mermaidcpp

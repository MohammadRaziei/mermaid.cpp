#include <cstring>
#include <string>

#include "mermaidcpp.h"
#include "utest.h"

UTEST(version, macros_form_the_string) {
    ASSERT_STREQ("0.1.0", MERMAIDCPP_VERSION_STRING);
    ASSERT_EQ(100, MERMAIDCPP_VERSION);
}

UTEST(version, function_matches_macro) {
    ASSERT_STREQ(MERMAIDCPP_VERSION_STRING, mermaidcpp::version());
}

UTEST(version, pinned_mermaid_reference) {
    ASSERT_STREQ("11.16.0", mermaidcpp::mermaid_reference_version());
}

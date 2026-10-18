#include <cstring>
#include <string>

#include <mermaid/mermaid.h>
#include "utest.h"

UTEST(version, macros_form_the_string) {
    ASSERT_STREQ("0.1.0", MERMAID_VERSION_STRING);
    ASSERT_EQ(100, MERMAID_VERSION);
}

UTEST(version, function_matches_macro) {
    ASSERT_STREQ(MERMAID_VERSION_STRING, mermaid::version());
}

UTEST(version, pinned_mermaid_reference) {
    ASSERT_STREQ("12.2.0", mermaid::mermaid_reference_version());
}

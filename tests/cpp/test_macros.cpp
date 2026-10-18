#include <mermaid/macros.h>

#include "utest.h"

// Windows headers define function-like min/max macros; the MERMAID_* spellings must survive them.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#define min(a, b) (broken)
#define max(a, b) (broken)

UTEST(macros, min_and_max_survive_function_like_min_max_macros) {
    ASSERT_EQ(2, MERMAID_MIN(2, 5));
    ASSERT_EQ(5, MERMAID_MAX(2, 5));
    ASSERT_NEAR(1.5, MERMAID_MIN(1.5, 2.5), 1e-12);
    ASSERT_NEAR(2.5, MERMAID_MAX(1.5, 2.5), 1e-12);
}

#undef min
#undef max
#pragma pop_macro("min")
#pragma pop_macro("max")

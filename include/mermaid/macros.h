// macros.h - small portability macros shared by the library.
#pragma once

#include <algorithm>

// <windows.h> defines function-like `min` / `max` macros that rewrite std::min(a, b) before the
// compiler sees it. Wrapping the name in parentheses stops a function-like macro from expanding,
// so these spellings are safe whether or not NOMINMAX is set, and keep std::min/std::max's type
// checking and single evaluation of the arguments.
#define MERMAID_MIN (std::min)
#define MERMAID_MAX (std::max)

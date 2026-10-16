// mermaid.h - public API of mermaid.cpp
//
// A dependency-free C++17 implementation of Mermaid diagrams.
// Behavioural reference: Mermaid 11.16.0 (see vendor/mermaid and the merman
// project).
//
// Version: single source of truth is the three macros below.
//   - cmake/DynamicVersion.cmake reads them to set PROJECT_VERSION.
#pragma once

#define MERMAID_VERSION_MAJOR 0
#define MERMAID_VERSION_MINOR 1
#define MERMAID_VERSION_PATCH 0

#define MERMAID_VERSION_ENCODE(maj, min, pat) (((maj) * 10000) + ((min) * 100) + (pat))
#define MERMAID_VERSION                                                      \
    MERMAID_VERSION_ENCODE(MERMAID_VERSION_MAJOR, MERMAID_VERSION_MINOR, \
                              MERMAID_VERSION_PATCH)

/* Internal helpers for MERMAID_VERSION_STRING */
#define MERMAID_DETAIL_VERSION_XSTR(a, b, c) #a "." #b "." #c
#define MERMAID_DETAIL_VERSION_STR(a, b, c) MERMAID_DETAIL_VERSION_XSTR(a, b, c)
#define MERMAID_VERSION_STRING                                               \
    MERMAID_DETAIL_VERSION_STR(MERMAID_VERSION_MAJOR, MERMAID_VERSION_MINOR, \
                            MERMAID_VERSION_PATCH)

/* Mermaid release this library tracks (merman pins the same one). */
#define MERMAID_REFERENCE_VERSION "11.16.0"

namespace mermaid {

/** Library version, "MAJOR.MINOR.PATCH". */
const char* version() noexcept;

/** Version of upstream Mermaid whose behaviour this library targets. */
const char* mermaid_reference_version() noexcept;

}  // namespace mermaid

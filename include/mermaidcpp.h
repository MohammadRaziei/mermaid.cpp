// mermaidcpp.h - public API of mermaid.cpp
//
// A dependency-free C++17 implementation of Mermaid diagrams.
// Behavioural reference: Mermaid 11.16.0 (see vendor/mermaid and the merman
// project).
//
// Version: single source of truth is the three macros below.
//   - cmake/DynamicVersion.cmake reads them to set PROJECT_VERSION.
#pragma once

#define MERMAIDCPP_VERSION_MAJOR 0
#define MERMAIDCPP_VERSION_MINOR 1
#define MERMAIDCPP_VERSION_PATCH 0

#define MERMAIDCPP_VERSION_ENCODE(maj, min, pat) (((maj) * 10000) + ((min) * 100) + (pat))
#define MERMAIDCPP_VERSION                                                      \
    MERMAIDCPP_VERSION_ENCODE(MERMAIDCPP_VERSION_MAJOR, MERMAIDCPP_VERSION_MINOR, \
                              MERMAIDCPP_VERSION_PATCH)

/* Internal helpers for MERMAIDCPP_VERSION_STRING */
#define _MERMAIDCPP_VERSION_XSTR(a, b, c) #a "." #b "." #c
#define _MERMAIDCPP_VERSION_STR(a, b, c) _MERMAIDCPP_VERSION_XSTR(a, b, c)
#define MERMAIDCPP_VERSION_STRING                                               \
    _MERMAIDCPP_VERSION_STR(MERMAIDCPP_VERSION_MAJOR, MERMAIDCPP_VERSION_MINOR, \
                            MERMAIDCPP_VERSION_PATCH)

/* Mermaid release this library tracks (merman pins the same one). */
#define MERMAIDCPP_MERMAID_REFERENCE_VERSION "11.16.0"

namespace mermaidcpp {

/** Library version, "MAJOR.MINOR.PATCH". */
const char* version() noexcept;

/** Version of upstream Mermaid whose behaviour this library targets. */
const char* mermaid_reference_version() noexcept;

}  // namespace mermaidcpp

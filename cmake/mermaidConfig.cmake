# mermaidConfig.cmake
#
# Usage (CMake >= 3.15):
#
#   find_package(mermaid CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE mermaid::mermaid)
#
# `mermaid::mermaid` is the same target you get from
# FetchContent/add_subdirectory. It carries the compiled static library and
# the include directory for <mermaid.h>. Paths resolve relative to this
# file, so the package is relocatable.
#
# Plain-path variables for build systems that do not use targets:
#
#   mermaid_LIB_PATH       full path of the compiled library
#   mermaid_INCLUDE_PATH   directory containing mermaid.h

include("${CMAKE_CURRENT_LIST_DIR}/mermaidTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/mermaidPaths.cmake")   # generated at build time

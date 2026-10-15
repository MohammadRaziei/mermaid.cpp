# mermaidcppConfig.cmake
#
# Usage (CMake >= 3.15):
#
#   find_package(mermaidcpp CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE mermaidcpp::mermaidcpp)
#
# `mermaidcpp::mermaidcpp` is the same target you get from
# FetchContent/add_subdirectory. It carries the compiled static library and
# the include directory for <mermaidcpp.h>. Paths resolve relative to this
# file, so the package is relocatable.
#
# Plain-path variables for build systems that do not use targets:
#
#   mermaidcpp_LIB_PATH       full path of the compiled library
#   mermaidcpp_INCLUDE_PATH   directory containing mermaidcpp.h

include("${CMAKE_CURRENT_LIST_DIR}/mermaidcppTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/mermaidcppPaths.cmake")   # generated at build time

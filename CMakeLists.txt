cmake_minimum_required(VERSION 3.10)
project(mermaid_cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(mermaid
    src/mermaid.cpp
    src/lexer.cpp
    src/parser.cpp
    src/layout.cpp
    src/renderer.cpp
    src/ast.cpp
)

target_include_directories(mermaid PUBLIC includes PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

add_executable(mermaidc
    src/cli.cpp
)

target_link_libraries(mermaidc PRIVATE mermaid)

file(GLOB_RECURSE sequence_test_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_*.cpp")

add_executable(sequence_tests
    ${sequence_test_sources}
)

target_include_directories(sequence_tests PRIVATE includes ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(sequence_tests PRIVATE mermaid)

enable_testing()
add_test(NAME sequence_svg COMMAND sequence_tests)

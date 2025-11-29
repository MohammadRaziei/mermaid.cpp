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

target_include_directories(mermaid PUBLIC includes)

add_executable(mermaid_cli
    src/cli.cpp
)

target_link_libraries(mermaid_cli PRIVATE mermaid)

add_executable(sequence_tests
    tests/test_sequence_svg.cpp
)

target_include_directories(sequence_tests PRIVATE includes)
target_link_libraries(sequence_tests PRIVATE mermaid)

enable_testing()
add_test(NAME sequence_svg COMMAND sequence_tests)

set(MERMAID_TEST_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MERMAID_TEST_BINARY_DIR ${CMAKE_BINARY_DIR}/tests)
set(MERMAID_COVERAGE_BINARY_DIR ${CMAKE_BINARY_DIR}/coverage)
set(MERMAID_COVERAGE_CPP_BINARY_DIR ${MERMAID_COVERAGE_BINARY_DIR}/cpp CACHE PATH "C++ coverage output directory")

file(MAKE_DIRECTORY ${MERMAID_COVERAGE_BINARY_DIR})

add_custom_target(mermaid_test
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running All tests..."
)
add_custom_target(mermaid_coverage
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running coverage ..."
)

find_package(Threads REQUIRED)
find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)

add_subdirectory(${MERMAID_TEST_SOURCE_DIR}/cpp)

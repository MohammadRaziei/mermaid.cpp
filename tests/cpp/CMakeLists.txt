# Find all test source files matching test_*.cpp pattern
file(GLOB MERMAID_TEST_SOURCES_CPP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Library sources are compiled straight into the test binary (not linked as
# the static lib) so coverage instrumentation applies to them.
add_executable(test_mermaid_cpp
    ${MERMAID_TEST_SOURCES_CPP}
    ${SOURCES}
    ${MERMAID_TEST_SOURCE_DIR}/utest/utest_main.cxx
)

target_include_directories(test_mermaid_cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MERMAID_TEST_SOURCE_DIR}/utest
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_definitions(test_mermaid_cpp PRIVATE
    MERMAID_TEST_DATA_DIR="${MERMAID_TEST_SOURCE_DIR}/data"
)

add_test(NAME test_mermaid_cpp COMMAND $<TARGET_FILE:test_mermaid_cpp>)

add_custom_target(mermaid_test_cpp
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C++ tests..."
    COMMAND $<TARGET_FILE:test_mermaid_cpp>
    WORKING_DIRECTORY ${MERMAID_TEST_BINARY_DIR}
)
add_dependencies(mermaid_test_cpp test_mermaid_cpp)
add_dependencies(mermaid_test mermaid_test_cpp)

if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    target_compile_options(test_mermaid_cpp PRIVATE -O0 -g --coverage)
    target_link_options(test_mermaid_cpp PRIVATE --coverage)

    add_custom_target(mermaid_coverage_cpp
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERMAID_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MERMAID_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${LCOV_EXECUTABLE}
                --capture
                --base-directory ${PROJECT_SOURCE_DIR}
                --directory ${CMAKE_BINARY_DIR}
                --exclude "*/tests/*"
                --no-external
                --rc function_coverage=1
                --output-file ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${LCOV_EXECUTABLE}
                --extract ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
                "*/include/*" "*/src/*"
                --output-file ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND} -E remove ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                --output-directory ${MERMAID_COVERAGE_CPP_BINARY_DIR}/html
                --prefix ${PROJECT_SOURCE_DIR}
                --title "mermaid C++ Coverage"
                --css-file ${MERMAID_TEST_SOURCE_DIR}/genhtml.css
                --dark-mode
        COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
        COMMAND ${LCOV_EXECUTABLE} --list ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        WORKING_DIRECTORY ${MERMAID_TEST_BINARY_DIR}
        USES_TERMINAL
    )
    add_dependencies(mermaid_coverage_cpp mermaid_test_cpp)
    add_dependencies(mermaid_coverage mermaid_coverage_cpp)
else()
    message(WARNING "LCOV or GENHTML not found - C++ coverage target skipped "
                    "(e.g. 'sudo apt install lcov').")
endif()
